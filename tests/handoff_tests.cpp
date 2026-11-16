#include <QDir>
#include <QFile>
#include <QSettings>
#include <QTemporaryDir>
#include <QtTest>

#include "backend.h"
#include "receiver.h"
#include "rsync.h"
#include "theme.h"
#include "translator.h"

// Answers the file chooser the way a user would.
class FakePicker : public FilePicker {
public:
    QStringList answer;
    bool askedForFolders = false;
    void pick(bool folders, const QString &) override
    {
        askedForFolders = folders;
        emit picked(answer);
    }
};

class HandoffTests : public QObject {
    Q_OBJECT

private slots:
    void mirrorsTheHomeFolder()
    {
        Rsync::Plan plan;
        plan.target = "ana@192.168.1.20";
        plan.home = "/home/ana";
        plan.sources = {"/home/ana/Documents/tax", "/home/ana/Pictures/"};
        const auto command = Rsync::build(plan);
        QVERIFY2(command.error.isEmpty(), qPrintable(command.error));
        QVERIFY(command.arguments.contains("--relative"));
        QVERIFY(command.arguments.contains("--update"));
        QVERIFY(!command.arguments.contains("--dry-run"));
        QVERIFY(!command.arguments.contains("--delete"));
        const int separator = command.arguments.indexOf("--");
        QCOMPARE(command.arguments.mid(separator + 1),
                 QStringList({"/home/ana/./Documents/tax", "/home/ana/./Pictures", "ana@192.168.1.20:"}));
    }

    void sendsIntoAFolder()
    {
        Rsync::Plan plan;
        plan.target = "ana@new.local";
        plan.home = "/home/ana";
        plan.sources = {"/mnt/disk/films"};
        plan.mirrorHome = false;
        plan.into = "~/From old laptop";
        plan.keepNewer = false;
        plan.dryRun = true;
        const auto command = Rsync::build(plan);
        QVERIFY2(command.error.isEmpty(), qPrintable(command.error));
        QVERIFY(!command.arguments.contains("--relative"));
        QVERIFY(!command.arguments.contains("--update"));
        QVERIFY(command.arguments.contains("--dry-run"));
        QCOMPARE(command.arguments.last(), QString("ana@new.local:From old laptop/"));
        QCOMPARE(command.arguments.at(command.arguments.size() - 2), QString("/mnt/disk/films"));
    }

    void refusesWhatItCannotSendSafely()
    {
        Rsync::Plan plan;
        plan.home = "/home/ana";
        plan.sources = {"/home/ana/Documents"};
        QVERIFY(!Rsync::build(plan).error.isEmpty());

        plan.target = "-oProxyCommand=evil";
        QVERIFY(!Rsync::build(plan).error.isEmpty());
        plan.target = "ana@host; rm -rf ~";
        QVERIFY(!Rsync::build(plan).error.isEmpty());

        plan.target = "ana@192.168.1.20";
        plan.sources = {"/etc/hosts"};
        QVERIFY(Rsync::build(plan).error.contains("/etc/hosts"));
        plan.sources = {"/home/ana"};
        QVERIFY(!Rsync::build(plan).error.isEmpty());
        plan.sources = {"/home/anabel/Documents"};
        QVERIFY(!Rsync::build(plan).error.isEmpty());
        plan.sources.clear();
        QVERIFY(!Rsync::build(plan).error.isEmpty());

        plan.sources = {"/home/ana/Documents"};
        plan.mirrorHome = false;
        plan.into = " ";
        QVERIFY(!Rsync::build(plan).error.isEmpty());
    }

    void batchModeNeverAsksForAPassword()
    {
        QVERIFY(Rsync::sshOptions(true).contains("BatchMode=yes"));
        QVERIFY(!Rsync::sshOptions(false).contains("BatchMode=yes"));
    }

    void readsProgressAndStatistics()
    {
        const auto progress = Rsync::parseProgress("  1,234,567  45%   10.50MB/s    0:01:23 (xfr#12, to-chk=100/200)");
        QVERIFY(progress.valid);
        QCOMPARE(progress.bytes, 1234567);
        QCOMPARE(progress.percent, 45);
        QCOMPARE(progress.bytesPerSecond, 10500000);
        QCOMPARE(progress.remaining, QString("0:01:23"));
        QVERIFY(!Rsync::parseProgress("Documents/tax/2024.pdf").valid);

        const auto newer = Rsync::parseProgress("      3,000,002B  62% 41,200,000B/s    0:00:09 (xfr#2, to-chk=0/5)");
        QVERIFY(newer.valid);
        QCOMPARE(newer.bytes, 3000002);
        QCOMPARE(newer.percent, 62);
        QCOMPARE(newer.bytesPerSecond, 41200000);

        const auto stats = Rsync::parseStats("Number of files: 40 (reg: 30, dir: 10)\n"
                                             "Number of regular files transferred: 1,204\n"
                                             "Total file size: 9,000,000 bytes\n"
                                             "Total transferred file size: 3,500,000 bytes\n");
        QCOMPARE(stats.files, 1204);
        QCOMPARE(stats.bytes, 3500000);
    }

    void explainsFailures()
    {
        QVERIFY(Rsync::explain(255, "ana@host: Permission denied (publickey,password).").contains("password"));
        QVERIFY(Rsync::explain(255, "ssh: connect to host 10.0.0.2 port 22: Connection refused").contains("receiving"));
        QVERIFY(Rsync::explain(23, "").contains("Some files"));
    }

    void keepsAListWithUndo()
    {
        QTemporaryDir dir;
        QDir(dir.path()).mkpath("a");
        QDir(dir.path()).mkpath("b");
        FakePicker picker;
        Backend backend(&picker, dir.filePath("settings.conf"));

        picker.answer = {dir.filePath("a"), dir.filePath("b")};
        backend.addFolders();
        QVERIFY(picker.askedForFolders);
        QCOMPARE(backend.sources().size(), 2);

        // The same folder twice is still one thing to send.
        backend.addPaths({dir.filePath("a") + "/"});
        QCOMPARE(backend.sources().size(), 2);

        backend.removeAt(0);
        QCOMPARE(backend.sources(), QStringList{dir.filePath("b")});
        backend.undo();
        QCOMPARE(backend.sources().size(), 2);
        backend.redo();
        QCOMPARE(backend.sources().size(), 1);

        backend.addPaths({dir.filePath("missing")});
        QCOMPARE(backend.state(), QString("failed"));
        QCOMPARE(backend.sources().size(), 1);
    }

    void remembersTheAddressButNotThePassword()
    {
        QTemporaryDir dir;
        const QString settings = dir.filePath("settings.conf");
        {
            Backend backend(nullptr, settings);
            backend.setTarget(" ana@192.168.1.20 ");
            backend.setPassword("secret");
            backend.setKeepNewer(false);
        }
        Backend backend(nullptr, settings);
        QCOMPARE(backend.target(), QString("ana@192.168.1.20"));
        QVERIFY(backend.password().isEmpty());
        QVERIFY(!backend.keepNewer());
        QFile file(settings);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QVERIFY(!file.readAll().contains("secret"));
    }

    void sendsAndReportsProgress()
    {
        QTemporaryDir dir;
        const QString rsync = script(dir, "rsync",
            "printf 'tax/2024.pdf\\n'\n"
            "printf '  1,000  50%%  1.00MB/s  0:00:01\\r'\n"
            "printf '  2,000 100%%  1.00MB/s  0:00:00 (xfr#2, to-chk=0/3)\\n'\n"
            "printf '\\nNumber of files: 3\\nNumber of regular files transferred: 2\\n"
            "Total transferred file size: 2,000 bytes\\n'\n"
            "printf '%s\\n' \"$@\" > '" + dir.filePath("arguments") + "'\n");
        Backend backend(nullptr, dir.filePath("settings.conf"), rsync);
        backend.setTarget("ana@192.168.1.20");
        backend.setMirrorHome(false);
        backend.setInto("Inbox");
        backend.addPaths({dir.path()});

        backend.send();
        QVERIFY(backend.running());
        QTRY_VERIFY(!backend.running());
        QCOMPARE(backend.state(), QString("done"));
        QCOMPARE(backend.percent(), 100);
        QVERIFY2(backend.message().contains("files sent"), qPrintable(backend.message()));

        QFile arguments(dir.filePath("arguments"));
        QVERIFY(arguments.open(QIODevice::ReadOnly));
        QVERIFY(arguments.readAll().trimmed().endsWith("ana@192.168.1.20:Inbox/"));
    }

    void reportsAFailedSend()
    {
        QTemporaryDir dir;
        const QString rsync = script(dir, "rsync",
            "echo 'ana@192.168.1.20: Permission denied (publickey,password).' >&2\nexit 255\n");
        Backend backend(nullptr, dir.filePath("settings.conf"), rsync);
        backend.setTarget("ana@192.168.1.20");
        backend.setMirrorHome(false);
        backend.setInto("Inbox");
        backend.addPaths({dir.path()});
        backend.preview();
        QTRY_VERIFY(!backend.running());
        QCOMPARE(backend.state(), QString("failed"));
        QVERIFY(backend.detail().contains("Permission denied"));
    }

    void doesNotStartWithoutAnAddress()
    {
        QTemporaryDir dir;
        Backend backend(nullptr, dir.filePath("settings.conf"), "/nonexistent/rsync");
        backend.addPaths({dir.path()});
        backend.send();
        QVERIFY(!backend.running());
        QCOMPARE(backend.state(), QString("failed"));
    }

    void readsTheFirewallRules()
    {
        QVERIFY(!Receiver::rulesAllowSsh("### tuple ### allow tcp 53317 0.0.0.0/0 any 0.0.0.0/0 in\n"
                                         "### tuple ### allow udp 53 172.17.0.1 any 172.16.0.0/12 in comment=61\n"));
        QVERIFY(Receiver::rulesAllowSsh("### tuple ### allow tcp 53317 0.0.0.0/0 any 0.0.0.0/0 in\n"
                                        "### tuple ### allow tcp 22 0.0.0.0/0 any 192.168.1.0/24 in comment=68\n"));
        QVERIFY(!Receiver::rulesAllowSsh("### tuple ### deny tcp 22 0.0.0.0/0 any 0.0.0.0/0 in\n"));
    }

    void worksOutTheNetwork()
    {
        QCOMPARE(Receiver::subnetOf("192.168.1.37", 24), QString("192.168.1.0/24"));
        QCOMPARE(Receiver::subnetOf("10.241.70.240", 21), QString("10.241.64.0/21"));
        QVERIFY(Receiver::subnetOf("fe80::1", 64).isEmpty());
    }

    void opensAndClosesOnlyWhatItOpened()
    {
        QTemporaryDir dir;
        const QString log = dir.filePath("log"), rules = dir.filePath("user.rules");
        const QString systemctl = script(dir, "systemctl", "echo inactive\necho active\n");
        // Stands in for pkexec: records what would have run as root.
        const QString pkexec = script(dir, "pkexec", "shift 4\necho \"$@\" >> '" + log + "'\n");
        write(rules, "### tuple ### allow tcp 53317 0.0.0.0/0 any 0.0.0.0/0 in\n");

        Receiver receiver(dir.filePath("settings.conf"), rules, systemctl, pkexec);
        QVERIFY(!receiver.sshRunning());
        QVERIFY(receiver.firewallActive());
        QVERIFY(!receiver.ready());
        if (receiver.subnet().isEmpty())
            QSKIP("No network on this machine.");

        receiver.open();
        QTRY_VERIFY(receiver.opened());
        receiver.close();
        QTRY_VERIFY(!receiver.opened());

        QFile file(log);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(QString::fromUtf8(file.readAll()),
                 QString("open %1\nclose %1 stop\n").arg(receiver.subnet()));
    }

    void reportsARefusedPassword()
    {
        QTemporaryDir dir;
        const QString systemctl = script(dir, "systemctl", "echo inactive\necho active\n");
        const QString pkexec = script(dir, "pkexec", "exit 126\n");
        Receiver receiver(dir.filePath("settings.conf"), dir.filePath("none"), systemctl, pkexec);
        if (receiver.subnet().isEmpty())
            QSKIP("No network on this machine.");
        receiver.open();
        QTRY_VERIFY(!receiver.error().isEmpty());
        QVERIFY(!receiver.opened());
    }

    void themeFallsBack()
    {
        QTemporaryDir dir;
        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#FFD60A"));
        QCOMPARE(theme.accentForeground(), QString("black"));
    }

    void themeReadsAccent()
    {
        QTemporaryDir dir;
        QDir(dir.path()).mkpath("theme");
        write(dir.path() + "/theme/colors.toml", "accent = \"#112233\"\n");

        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#112233"));
        QCOMPARE(theme.accentForeground(), QString("white"));
    }

    void themeFollowsHyprlandRounding()
    {
        QTemporaryDir dir;
        const QString response = dir.filePath("response.json");
        const QString command = script(dir, "hyprctl", "cat '" + response + "'\n");
        write(response, "{\"int\":0}");

        Theme theme(dir.path(), nullptr, command);
        QCOMPARE(theme.radius(), 0);
        write(response, "{\"int\":6}");
        QTRY_COMPARE(theme.radius(), 6);
    }

    void translatorFallsBackToEnglish()
    {
        QTemporaryDir dir;
        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.language(), QString("en"));
        QVERIFY(translator.translate("Main", "Send").isNull());
    }

    void translatorFollowsTheLanguageFile()
    {
        QTemporaryDir dir;
        write(dir.path() + "/language", "es-MX\n");
        write(dir.path() + "/es.json", R"({"Send": "Enviar", "Receive": "Recibir"})");
        write(dir.path() + "/es-MX.json", R"({"Send": "Mandar"})");

        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.language(), QString("es-MX"));
        QCOMPARE(translator.translate("Main", "Send"), QString("Mandar"));
        QCOMPARE(translator.translate("Main", "Receive"), QString("Recibir"));
        QVERIFY(translator.translate("Main", "Quit").isNull());
    }

    void shipsSpanishForEveryString()
    {
        QTemporaryDir dir;
        write(dir.path() + "/language", "es-MX\n");
        Translator translator(dir.path() + "/language", QString(SOURCE_DIRECTORY) + "/i18n");
        QCOMPARE(translator.translate("Main", "Send"), QString("Enviar"));
        QCOMPARE(translator.translate("Backend", "%n files sent (%1).", nullptr, 1), QString("%n archivo enviado (%1)."));
        QCOMPARE(translator.translate("Backend", "%n files sent (%1).", nullptr, 5), QString("%n archivos enviados (%1)."));
    }

private:
    static void write(const QString &path, const QByteArray &contents)
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(contents);
    }

    static QString script(const QTemporaryDir &dir, const QString &name, const QString &body)
    {
        const QString path = dir.filePath(name);
        QFile file(path);
        if (file.open(QIODevice::WriteOnly))
            file.write("#!/bin/sh\n" + body.toUtf8());
        file.close();
        QFile::setPermissions(path, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
        return path;
    }
};

QTEST_MAIN(HandoffTests)
#include "handoff_tests.moc"
