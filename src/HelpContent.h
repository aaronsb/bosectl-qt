#pragma once

#include <QFile>
#include <QHash>
#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <QTextBlock>
#include <QTextDocument>

// One headed section of help.md.
struct HelpEntry {
    QString id;
    QString title;
    QString body;     // Markdown, blank lines trimmed from both ends
    QString parent;   // enclosing frame (level 2) or item (level 3) id
    int level = 0;    // 1 = frame, 2 = item, 3 = choice
};

// The help text behind the Help window and the tray/dialog tooltips, parsed
// from resources/help.md (compiled in as :/help.md). The file is plain
// Markdown with an explicit id on every heading, so the text can be
// reworded freely while the UI keeps finding it:
//
//   # Title {#frame-id}     a frame: the tray menu or one of the dialogs
//   ## Title {#item-id}     an item in that frame
//   ### Title {#item-id}    a choice under the preceding item
//
// Everything up to the next #/##/### heading is the section's body. Text
// before the first heading is the intro. HTML comments are dropped, so the
// file can carry authoring notes. Headings inside fenced code blocks and
// #### or deeper are body text.
class HelpContent {
public:
    static HelpContent parse(const QString& markdown) {
        HelpContent c;
        static const QRegularExpression heading(
            R"(^(#{1,3})[ \t]+(.*?)[ \t]*\{#([^\s}]+)\}[ \t]*$)");
        static const QRegularExpression bareHeading(R"(^#{1,3}[ \t])");

        QString frame, item;
        // Where non-heading lines go. A rejected heading's body is dropped
        // rather than appended to whatever came before it.
        enum { Intro, Entry, Discard } target = Intro;
        QString current;
        bool fenced = false;
        int lineNo = 0;

        for (const QString& line : stripComments(markdown).split('\n')) {
            ++lineNo;
            const QString trimmed = line.trimmed();
            if (trimmed.startsWith("```") || trimmed.startsWith("~~~")) fenced = !fenced;

            if (!fenced && bareHeading.match(line).hasMatch()) {
                const auto m = heading.match(line);
                if (!m.hasMatch()) {
                    c.errors_ << QString("line %1: heading has no {#id}").arg(lineNo);
                    target = Discard;
                    continue;
                }
                HelpEntry e;
                e.level = m.captured(1).size();
                e.title = m.captured(2);
                e.id = m.captured(3);
                if (e.level == 1) {
                    frame = e.id;
                    item.clear();
                } else if (e.level == 2) {
                    e.parent = frame;
                    item = e.id;
                } else {
                    e.parent = item;
                }
                if (e.level > 1 && e.parent.isEmpty())
                    c.errors_ << QString("line %1: %2 has no enclosing heading").arg(lineNo).arg(e.id);
                if (c.entries_.contains(e.id)) {
                    c.errors_ << QString("line %1: duplicate id %2").arg(lineNo).arg(e.id);
                    target = Discard;
                    continue;
                }
                c.order_ << e.id;
                c.entries_.insert(e.id, e);
                target = Entry;
                current = e.id;
                continue;
            }
            if (target == Intro) c.intro_ += line + '\n';
            else if (target == Entry) c.entries_[current].body += line + '\n';
        }

        c.intro_ = trimBlankLines(c.intro_);
        for (auto& e : c.entries_) e.body = trimBlankLines(e.body);
        return c;
    }

    static HelpContent load(const QString& path = QStringLiteral(":/help.md")) {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            HelpContent c;
            c.errors_ << QString("cannot read %1").arg(path);
            return c;
        }
        return parse(QString::fromUtf8(f.readAll()));
    }

    // The compiled-in help, parsed once on first use.
    static const HelpContent& shared() {
        static const HelpContent content = load();
        return content;
    }

    bool contains(const QString& id) const { return entries_.contains(id); }
    HelpEntry entry(const QString& id) const { return entries_.value(id); }
    QString title(const QString& id) const { return entries_.value(id).title; }
    QString body(const QString& id) const { return entries_.value(id).body; }
    QStringList ids() const { return order_; }   // document order
    QString intro() const { return intro_; }
    QStringList errors() const { return errors_; }

    // Tooltip text for a control: the first sentence of its help body as
    // plain text. Empty when there is no entry, so a missing id clears the
    // tooltip rather than showing something stale.
    QString tooltip(const QString& id) const {
        return firstSentence(body(id));
    }

    // First sentence of the first paragraph of a Markdown body, with the
    // Markdown rendered away (links become their text, emphasis is
    // dropped). A '.' inside a token like "e.g." does not end a sentence.
    static QString firstSentence(const QString& markdown) {
        if (markdown.trimmed().isEmpty()) return {};
        QTextDocument doc;
        doc.setMarkdown(markdown);
        const QString text = doc.firstBlock().text().simplified();

        for (qsizetype i = 0; i < text.size(); ++i) {
            const QChar ch = text[i];
            if (ch != '.' && ch != '!' && ch != '?') continue;
            if (i + 1 < text.size() && !text[i + 1].isSpace()) continue;
            if (ch == '.') {
                const qsizetype start = text.lastIndexOf(' ', i) + 1;
                if (text.mid(start, i - start).contains('.')) continue;   // e.g. / i.e.
            }
            return text.left(i + 1);
        }
        return text;
    }

private:
    // Each comment is replaced by the newlines it spanned, so line numbers
    // in errors() still match the file.
    static QString stripComments(QString s) {
        qsizetype from = 0;
        while ((from = s.indexOf("<!--", from)) >= 0) {
            qsizetype end = s.indexOf("-->", from + 4);
            end = end < 0 ? s.size() : end + 3;
            const QString newlines(s.mid(from, end - from).count('\n'), '\n');
            s.replace(from, end - from, newlines);
            from += newlines.size();
        }
        return s;
    }

    static QString trimBlankLines(const QString& s) {
        QStringList lines = s.split('\n');
        while (!lines.isEmpty() && lines.first().trimmed().isEmpty()) lines.removeFirst();
        while (!lines.isEmpty() && lines.last().trimmed().isEmpty()) lines.removeLast();
        return lines.join('\n');
    }

    QHash<QString, HelpEntry> entries_;
    QStringList order_;
    QString intro_;
    QStringList errors_;
};
