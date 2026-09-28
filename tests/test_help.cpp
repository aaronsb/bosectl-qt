// help.md parsing, and the contract between help.md and the Help window:
// every id the window shows has text, and every heading in help.md is
// shown somewhere, so neither can drift from the other.
#include <QtTest>
#include <QPushButton>
#include <QSet>
#include <QSlider>

#include "HelpContent.h"
#include "HelpWindow.h"

class TestHelp : public QObject {
    Q_OBJECT

private:
    static QWidget* widgetFor(HelpWindow& w, const QString& id) {
        for (QWidget* child : w.findChildren<QWidget*>())
            if (child->property("helpId").toString() == id) return child;
        return nullptr;
    }

private slots:
    // ── Parser ──────────────────────────────────────────────────────────────

    void parsesFramesItemsAndChoices() {
        const auto c = HelpContent::parse(
            "<!--\n"
            "# Hidden {#hidden}\n"
            "-->\n"
            "Welcome to **help**.\n"
            "\n"
            "# Tray menu {#menu}\n"
            "The menu.\n"
            "## Spatial Audio {#menu.spatial}\n"
            "\n"
            "Picks a *mode*.\n"
            "\n"
            "- one\n"
            "- two\n"
            "\n"
            "### Room {#menu.spatial.room}\n"
            "Fixed in space.\n"
            "## Quit {#menu.quit}\n"
            "# Equalizer {#eq}\n"
            "## Bass {#eq.bass}\n"
            "Low end.\n");

        QVERIFY2(c.errors().isEmpty(), qPrintable(c.errors().join("; ")));
        QCOMPARE(c.ids(), (QStringList{"menu", "menu.spatial", "menu.spatial.room",
                                       "menu.quit", "eq", "eq.bass"}));
        QVERIFY(!c.contains("hidden"));
        QCOMPARE(c.intro(), QString("Welcome to **help**."));

        QCOMPARE(c.entry("menu").level, 1);
        QCOMPARE(c.entry("menu").body, QString("The menu."));

        const HelpEntry spatial = c.entry("menu.spatial");
        QCOMPARE(spatial.level, 2);
        QCOMPARE(spatial.title, QString("Spatial Audio"));
        QCOMPARE(spatial.parent, QString("menu"));
        QCOMPARE(spatial.body, QString("Picks a *mode*.\n\n- one\n- two"));

        const HelpEntry room = c.entry("menu.spatial.room");
        QCOMPARE(room.level, 3);
        QCOMPARE(room.parent, QString("menu.spatial"));
        QCOMPARE(room.body, QString("Fixed in space."));

        QCOMPARE(c.body("menu.quit"), QString());
        QCOMPARE(c.entry("eq.bass").parent, QString("eq"));
    }

    void deepHeadingsAndCodeFencesAreBody() {
        const auto c = HelpContent::parse(
            "# F {#f}\n"
            "## Item {#f.item}\n"
            "#### Detail\n"
            "```\n"
            "## not a heading {#f.fake}\n"
            "```\n");
        QVERIFY(c.errors().isEmpty());
        QVERIFY(!c.contains("f.fake"));
        QVERIFY(c.body("f.item").startsWith("#### Detail\n```"));
        QVERIFY(c.body("f.item").contains("{#f.fake}"));
    }

    void reportsMalformedHeadings() {
        const auto c = HelpContent::parse(
            "<!-- two\nlines -->\n"
            "# F {#f}\n"
            "## No id here\n"
            "dropped\n"
            "## A {#f.a}\n"
            "## Again {#f.a}\n"
            "### Orphan {#g.x}\n");
        // Line numbers count the comment's lines, so they match the file.
        QCOMPARE(c.errors().size(), 2);
        QVERIFY(c.errors()[0].startsWith("line 4:"));
        QVERIFY(c.errors()[1].contains("duplicate id f.a"));
        QCOMPARE(c.title("f.a"), QString("A"));
        QVERIFY(!c.body("f").contains("dropped"));

        const auto orphan = HelpContent::parse("## Loose {#x}\n");
        QCOMPARE(orphan.errors().size(), 1);
    }

    void firstSentenceStripsMarkdown() {
        QCOMPARE(HelpContent::firstSentence("Turns **ANC** on. Then more."),
                 QString("Turns ANC on."));
        QCOMPARE(HelpContent::firstSentence("See [the docs](https://example.com)! Later."),
                 QString("See the docs!"));
        QCOMPARE(HelpContent::firstSentence("Pick one, e.g. Room. Or not."),
                 QString("Pick one, e.g. Room."));
        QCOMPARE(HelpContent::firstSentence("Version 1.2 is fine\nacross lines"),
                 QString("Version 1.2 is fine across lines"));
        QCOMPARE(HelpContent::firstSentence("First para\n\nSecond. para"),
                 QString("First para"));
        QCOMPARE(HelpContent::firstSentence(""), QString());
    }

    // ── help.md ↔ window contract ──────────────────────────────────────────

    void shippedHelpParsesCleanly() {
        const auto c = HelpContent::load();
        QVERIFY2(c.errors().isEmpty(), qPrintable(c.errors().join("; ")));
        QVERIFY(!c.ids().isEmpty());
    }

    void helpAndWindowUseTheSameIds() {
        const auto file = HelpContent::load().ids();
        const QSet<QString> inFile(file.begin(), file.end());
        const auto window = HelpWindow::helpIds();
        const QSet<QString> inWindow(window.begin(), window.end());

        const QStringList missing = QSet<QString>(inWindow).subtract(inFile).values();
        const QStringList unused = QSet<QString>(inFile).subtract(inWindow).values();
        QVERIFY2(missing.isEmpty(),
                 qPrintable("shown but not in help.md: " + missing.join(", ")));
        QVERIFY2(unused.isEmpty(),
                 qPrintable("in help.md but never shown: " + unused.join(", ")));
    }

    void windowBindsEveryListedId() {
        HelpWindow w;
        QSet<QString> bound;
        for (QWidget* child : w.findChildren<QWidget*>()) {
            const QString id = child->property("helpId").toString();
            if (!id.isEmpty()) bound.insert(id);
        }
        QSet<QString> expected;
        for (const QString& id : HelpWindow::helpIds())
            if (id.contains('.')) expected.insert(id);   // frames are titles, not items
        QCOMPARE(bound, expected);
    }

    // ── Behaviour ───────────────────────────────────────────────────────────

    void clickTogglesOneExplanation() {
        HelpWindow w;
        w.show();

        auto* multipoint = widgetFor(w, "menu.multipoint");
        auto* quit = widgetFor(w, "menu.quit");
        QVERIFY(multipoint && quit);

        QTest::mouseClick(multipoint, Qt::LeftButton);
        QCOMPARE(w.openId(), QString("menu.multipoint"));
        QTest::mouseClick(quit, Qt::LeftButton);
        QCOMPARE(w.openId(), QString("menu.quit"));
        QTest::mouseClick(quit, Qt::LeftButton);
        QCOMPARE(w.openId(), QString());
    }

    void replicaControlsAreInert() {
        HelpWindow w;
        w.show();

        auto* bass = qobject_cast<QSlider*>(widgetFor(w, "eq.bass"));
        QVERIFY(bass);
        const int before = bass->value();
        QTest::mouseClick(bass, Qt::LeftButton, {}, QPoint(bass->width() / 2, 2));
        QCOMPARE(w.openId(), QString("eq.bass"));
        QCOMPARE(bass->value(), before);

        bass->setFocus();
        QTest::keyClick(bass, Qt::Key_Up);
        QCOMPARE(bass->value(), before);
        QTest::keyClick(bass, Qt::Key_Space);   // Space toggles, like a click
        QCOMPARE(w.openId(), QString());
    }

    void submenuExpandsInline() {
        HelpWindow w;
        w.show();

        auto* spatial = widgetFor(w, "menu.spatial");
        auto* room = widgetFor(w, "menu.spatial.room");
        QVERIFY(spatial && room);
        QVERIFY(!room->isVisibleTo(&w));

        QTest::mouseClick(spatial, Qt::LeftButton);
        QVERIFY(room->isVisibleTo(&w));
        QCOMPARE(w.openId(), QString("menu.spatial"));

        QTest::mouseClick(room, Qt::LeftButton);
        QCOMPARE(w.openId(), QString("menu.spatial.room"));

        // Collapsing the submenu closes the choice's explanation with it.
        QTest::mouseClick(spatial, Qt::LeftButton);
        QVERIFY(!room->isVisibleTo(&w));
        QCOMPARE(w.openId(), QString());
    }
};

QTEST_MAIN(TestHelp)
#include "test_help.moc"
