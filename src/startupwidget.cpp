/*
 * SPDX-FileCopyrightText: 2021 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "startupwidget.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

#include <KLocalizedString>
#include <KColorSchemeManager>
#include <KActionMenu>
#include <KConfigGroup>
#include <QMenu>

#include <settings.h>

StartUpWidget::StartUpWidget(QWidget *parent)
    : QWidget{parent}
{
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    auto mainVLayout = new QVBoxLayout(this);
    setLayout(mainVLayout);

    auto firstButtonsRow = new QWidget(this);
    auto firstButtonsRowLayout = new QHBoxLayout(firstButtonsRow);
    auto secondButtonsRow = new QWidget(this);
    auto secondButtonsRowLayout = new QHBoxLayout(secondButtonsRow);

    auto image = new QLabel(this);
#ifdef Q_OS_WIN32
    image->setPixmap(QIcon(":/icons/mangareader").pixmap(256));
#else
    image->setPixmap(QIcon::fromTheme("mangareader").pixmap(256));
#endif
    image->setAlignment(Qt::AlignCenter);

    mainVLayout->addSpacerItem(new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Expanding));
    mainVLayout->addWidget(image);
    mainVLayout->addWidget(firstButtonsRow);
    mainVLayout->addWidget(secondButtonsRow);
    mainVLayout->addSpacerItem(new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Expanding));
    mainVLayout->addSpacerItem(new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Expanding));

    auto addMangaFolderButton = new QPushButton(i18n("Add Manga Library Folder"), this);
    addMangaFolderButton->setIcon(QIcon::fromTheme("folder"));
    addMangaFolderButton->setIconSize(QSize(32, 32));
    addMangaFolderButton->setVisible(MangaReaderSettings::mangaFolders().isEmpty());
    connect(addMangaFolderButton, &QPushButton::clicked,
            this, &StartUpWidget::addMangaFolderClicked);

    auto openMangaFolderButton = new QPushButton(i18n("Open Manga Folder"), this);
    openMangaFolderButton->setIcon(QIcon::fromTheme("folder"));
    openMangaFolderButton->setIconSize(QSize(32, 32));
    connect(openMangaFolderButton, &QPushButton::clicked,
            this, &StartUpWidget::openMangaFolderClicked);

    auto openMangaArchiveButton = new QPushButton(i18n("Open Manga Archive"), this);
    auto fallbackIcon = QIcon::fromTheme("package-x-generic");
    openMangaArchiveButton->setIcon(QIcon::fromTheme("application-x-archive", fallbackIcon));
    openMangaArchiveButton->setIconSize(QSize(32, 32));
    connect(openMangaArchiveButton, &QPushButton::clicked,
            this, &StartUpWidget::openMangaArchiveClicked);

    firstButtonsRowLayout->addSpacerItem(new QSpacerItem(1, 1, QSizePolicy::Expanding));
    firstButtonsRowLayout->addWidget(addMangaFolderButton);
    firstButtonsRowLayout->addWidget(openMangaFolderButton);
    firstButtonsRowLayout->addWidget(openMangaArchiveButton);
    firstButtonsRowLayout->addSpacerItem(new QSpacerItem(1, 1, QSizePolicy::Expanding));

    auto settingsButton = new QPushButton(i18n("Settings"), this);
    settingsButton->setIcon(QIcon::fromTheme("configure"));
    settingsButton->setIconSize(QSize(32, 32));
    connect(settingsButton, &QPushButton::clicked,
            this, &StartUpWidget::openSettingsClicked);

    auto configureShortcutsButton = new QPushButton(i18n("Configure Shortcuts"), this);
    configureShortcutsButton->setIcon(QIcon::fromTheme("input-keyboard"));
    configureShortcutsButton->setIconSize(QSize(32, 32));
    connect(configureShortcutsButton, &QPushButton::clicked,
            this, &StartUpWidget::openShortcutsConfigClicked);

    auto *schemes = new KColorSchemeManager(this);
    auto config = KSharedConfig::openConfig("mangareader/mangareader.conf");
    KConfigGroup cg(config, "UiSettings");
    auto schemeName = cg.readEntry("ColorScheme", QString());

    KActionMenu *colorSchemeAction = schemes->createSchemeSelectionMenu(schemeName, this);
    colorSchemeAction->setPopupMode(QToolButton::InstantPopup);

    auto colorSchemeChangerButton = new QPushButton(i18n("Color Scheme"), this);
    colorSchemeChangerButton->setIcon(QIcon::fromTheme("kcolorchooser"));
    colorSchemeChangerButton->setIconSize(QSize(32, 32));
    colorSchemeChangerButton->setMenu(colorSchemeAction->menu());

    connect(colorSchemeAction->menu(), &QMenu::triggered, this, [=](QAction *triggeredAction) {
        KConfigGroup cg(config, "UiSettings");
        cg.writeEntry("ColorScheme", KLocalizedString::removeAcceleratorMarker(triggeredAction->text()));
        cg.sync();
    });


    secondButtonsRowLayout->addSpacerItem(new QSpacerItem(1, 1, QSizePolicy::Expanding));
    secondButtonsRowLayout->addWidget(settingsButton);
    secondButtonsRowLayout->addWidget(configureShortcutsButton);
    secondButtonsRowLayout->addWidget(colorSchemeChangerButton);
    secondButtonsRowLayout->addSpacerItem(new QSpacerItem(1, 1, QSizePolicy::Expanding));
}
