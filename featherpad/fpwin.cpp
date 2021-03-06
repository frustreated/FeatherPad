/*
 * Copyright (C) Pedram Pourang (aka Tsu Jan) 2014 <tsujan2000@gmail.com>
 *
 * FeatherPad is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * FeatherPad is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "singleton.h"
#include "ui_fp.h"
#include "encoding.h"
#include "filedialog.h"
#include "pref.h"
#include "loading.h"

#include <QMessageBox>
#include <QFontDialog>
#include <QPrintDialog>
#include <QToolTip>
#include <QDesktopWidget>
#include <fstream> // std::ofstream
#include <QPrinter>

#if defined Q_WS_X11 || defined Q_OS_LINUX
#include <QX11Info>
#endif

#include "x11.h"

namespace FeatherPad {

FPwin::FPwin (QWidget *parent):QMainWindow (parent), dummyWidget (NULL), ui (new Ui::FPwin)
{
    ui->setupUi (this);

    rightClicked_ = -1;
    closeAll_ = false;

    /* JumpTo bar*/
    ui->spinBox->hide();
    ui->label->hide();
    ui->checkBox->hide();

    /* status bar */
    QLabel *statusLabel = new QLabel();
    statusLabel->setTextInteractionFlags (Qt::TextSelectableByMouse);
    QToolButton *wordButton = new QToolButton();
    wordButton->setAutoRaise (true);
    wordButton->setToolButtonStyle (Qt::ToolButtonIconOnly);
    wordButton->setIconSize (QSize (16, 16));
    wordButton->setMaximumHeight (16);
    wordButton->setIcon (QIcon (":icons/view-refresh.svg"));
    //wordButton->setText (tr ("Refresh"));
    wordButton->setToolTip (tr ("Calculate number of words\n(For huge texts, this may be CPU-intensive.)"));
    connect (wordButton, &QAbstractButton::clicked, this, &FPwin::wordButtonStatus);
    ui->statusBar->addWidget (statusLabel);
    ui->statusBar->addWidget (wordButton);

    /* text unlocking */
    ui->actionEdit->setVisible (false);

    /* replace dock */
    ui->dockReplace->setVisible (false);

    lastFile_ = "";

    applyConfig();

    if (!static_cast<FPsingleton*>(qApp)->getConfig().getShowStatusbar())
        ui->statusBar->hide();
    newTab();

    aGroup_ = new QActionGroup (this);
    ui->actionUTF_8->setActionGroup (aGroup_);
    ui->actionUTF_16->setActionGroup (aGroup_);
    ui->actionWindows_Arabic->setActionGroup (aGroup_);
    ui->actionISO_8859_1->setActionGroup (aGroup_);
    ui->actionISO_8859_15->setActionGroup (aGroup_);
    ui->actionWindows_1252->setActionGroup (aGroup_);
    ui->actionCryllic_CP1251->setActionGroup (aGroup_);
    ui->actionCryllic_KOI8_U->setActionGroup (aGroup_);
    ui->actionCryllic_ISO_8859_5->setActionGroup (aGroup_);
    ui->actionChineese_BIG5->setActionGroup (aGroup_);
    ui->actionChinese_GB18030->setActionGroup (aGroup_);
    ui->actionJapanese_ISO_2022_JP->setActionGroup (aGroup_);
    ui->actionJapanese_ISO_2022_JP_2->setActionGroup (aGroup_);
    ui->actionJapanese_ISO_2022_KR->setActionGroup (aGroup_);
    ui->actionJapanese_CP932->setActionGroup (aGroup_);
    ui->actionJapanese_EUC_JP->setActionGroup (aGroup_);
    ui->actionKorean_CP949->setActionGroup (aGroup_);
    ui->actionKorean_CP1361->setActionGroup (aGroup_);
    ui->actionKorean_EUC_KR->setActionGroup (aGroup_);
    ui->actionOther->setActionGroup (aGroup_);

    ui->actionUTF_8->setChecked (true);
    ui->actionOther->setDisabled (true);

    connect (ui->actionNew, &QAction::triggered, this, &FPwin::newTab);
    connect (ui->actionDetach, &QAction::triggered, this, &FPwin::detachTab);
    connect (ui->actionClose, &QAction::triggered, this, &FPwin::closeTab);
    connect (ui->tabWidget, &QTabWidget::tabCloseRequested, this, &FPwin::closeTabAtIndex);
    connect (ui->actionOpen, &QAction::triggered, this, &FPwin::fileOpen);
    connect (ui->actionReload, &QAction::triggered, this, &FPwin::reload);
    connect (aGroup_, &QActionGroup::triggered, this, &FPwin::enforceEncoding);
    connect (ui->actionSave, &QAction::triggered, this, &FPwin::fileSave);
    connect (ui->actionSaveAs, &QAction::triggered, this, &FPwin::fileSave);
    connect (ui->actionSaveCodec, &QAction::triggered, this, &FPwin::fileSave);

    connect (ui->actionCut, &QAction::triggered, this, &FPwin::cutText);
    connect (ui->actionCopy, &QAction::triggered, this, &FPwin::copyText);
    connect (ui->actionPaste, &QAction::triggered, this, &FPwin::pasteText);
    connect (ui->actionDelete, &QAction::triggered, this, &FPwin::deleteText);
    connect (ui->actionSelectAll, &QAction::triggered, this, &FPwin::selectAllText);

    connect (ui->actionEdit, &QAction::triggered, this, &FPwin::makeEditable);

    connect (ui->actionUndo, &QAction::triggered, this, &FPwin::undoing);
    connect (ui->actionRedo, &QAction::triggered, this, &FPwin::redoing);

    connect (ui->tabWidget, &QTabWidget::currentChanged, this, &FPwin::tabSwitch);
    connect (static_cast<TabBar*>(ui->tabWidget->tabBar()), &TabBar::tabDropped, this, &FPwin::detachAndDropTab);
    ui->tabWidget->tabBar()->setContextMenuPolicy (Qt::CustomContextMenu);
    connect (ui->tabWidget->tabBar(), &QWidget::customContextMenuRequested, this, &FPwin::tabContextMenu);
    connect (ui->actionCloseAll, &QAction::triggered, this, &FPwin::closeAllTabs);
    connect (ui->actionCloseRight, &QAction::triggered, this, &FPwin::closeRightTabs);
    connect (ui->actionCloseLeft, &QAction::triggered, this, &FPwin::closeLeftTabs);
    connect (ui->actionCloseOther, &QAction::triggered, this, &FPwin::closeOtherTabs);

    connect (ui->actionFont, &QAction::triggered, this, &FPwin::fontDialog);

    connect (ui->toolButton_nxt, &QAbstractButton::clicked, this, &FPwin::find);
    connect (ui->toolButton_prv, &QAbstractButton::clicked, this, &FPwin::find);
    connect (ui->pushButton_whole, &QAbstractButton::clicked, this, &FPwin::setSearchFlags);
    connect (ui->pushButton_case, &QAbstractButton::clicked, this, &FPwin::setSearchFlags);
    connect (ui->actionFind, &QAction::triggered, this, &FPwin::showHideSearch);
    connect (ui->actionJump, &QAction::triggered, this, &FPwin::jumpTo);
    connect (ui->spinBox, &QAbstractSpinBox::editingFinished, this, &FPwin::goTo);

    connect (ui->actionLineNumbers, &QAction::toggled, this, &FPwin::showLN);
    connect (ui->actionWrap, &QAction::triggered, this, &FPwin::toggleWrapping);
    connect (ui->actionSyntax, &QAction::triggered, this, &FPwin::toggleSyntaxHighlighting);
    connect (ui->actionIndent, &QAction::triggered, this, &FPwin::toggleIndent);
    connect (ui->lineEdit, &QLineEdit::returnPressed, this, &FPwin::find);

    connect (ui->actionPreferences, &QAction::triggered, this, &FPwin::prefDialog);

    connect (ui->actionReplace, &QAction::triggered, this, &FPwin::replaceDock);
    connect (ui->toolButtonNext, &QAbstractButton::clicked, this, &FPwin::replace);
    connect (ui->toolButtonPrv, &QAbstractButton::clicked, this, &FPwin::replace);
    connect (ui->toolButtonAll, &QAbstractButton::clicked, this, &FPwin::replaceAll);
    connect (ui->dockReplace, &QDockWidget::visibilityChanged, this, &FPwin::closeReplaceDock);
    connect (ui->dockReplace, &QDockWidget::topLevelChanged, this, &FPwin::resizeDock);

    connect (ui->actionDoc, &QAction::triggered, this, &FPwin::docProp);
    connect (ui->actionPrint, &QAction::triggered, this, &FPwin::filePrint);

    connect (ui->actionAbout, &QAction::triggered, this, &FPwin::aboutDialog);
    connect (ui->actionHelp, &QAction::triggered, this, &FPwin::helpDoc);

    QShortcut *zoomin = new QShortcut (QKeySequence (tr ("Ctrl+=", "Zoom in")), this);
    QShortcut *zoomout = new QShortcut (QKeySequence (tr ("Ctrl+-", "Zoom out")), this);
    QShortcut *zoomzero = new QShortcut (QKeySequence (tr ("Ctrl+0", "Zoom default")), this);
    connect (zoomin, &QShortcut::activated, this, &FPwin::zoomIn);
    connect (zoomout, &QShortcut::activated, this, &FPwin::zoomOut);
    connect (zoomzero, &QShortcut::activated, this, &FPwin::zoomZero);

    QShortcut *fullscreen = new QShortcut (QKeySequence (tr ("F11", "Fullscreen")), this);
    QShortcut *defaultsize = new QShortcut (QKeySequence (tr ("Ctrl+Shift+W", "Default size")), this);
    connect (fullscreen, &QShortcut::activated, this, &FPwin::fullScreening);
    connect (defaultsize, &QShortcut::activated, this, &FPwin::defaultSize);

    /* this is a workaround for the RTL bug in QPlainTextEdit */
    QShortcut *align = new QShortcut (QKeySequence (tr ("Ctrl+Shift+A", "Alignment")), this);
    connect (align, &QShortcut::activated, this, &FPwin::align);

    dummyWidget = new QWidget();
    setAcceptDrops (true);
    setAttribute (Qt::WA_AlwaysShowToolTips);
}
/*************************/
FPwin::~FPwin()
{
    delete dummyWidget; dummyWidget = NULL;
    delete aGroup_; aGroup_ = NULL;
    delete ui; ui = NULL;
}
/*************************/
void FPwin::closeEvent (QCloseEvent *event)
{
    bool keep = closeTabs (-1, -1, false);
    if (keep)
        event->ignore();
    else
    {
        FPsingleton *singleton = static_cast<FPsingleton*>(qApp);
        Config& config = singleton->getConfig();
        if (config.getRemSize() && windowState() == Qt::WindowNoState)
            config.setWinSize (size());
        singleton->Wins.removeOne (this);
        event->accept();
    }
}
/*************************/
void FPwin::applyConfig()
{
    Config config = static_cast<FPsingleton*>(qApp)->getConfig();

    if (config.getRemSize())
    {
        resize (config.getWinSize());
        if (config.getIsMaxed())
            setWindowState (Qt::WindowMaximized);
        if (config.getIsFull() && config.getIsMaxed())
            setWindowState (windowState() ^ Qt::WindowFullScreen);
        else if (config.getIsFull())
            setWindowState (Qt::WindowFullScreen);
    }

    ui->mainToolBar->setVisible (!config.getNoToolbar());

    ui->lineEdit->setVisible (!config.getHideSearchbar());
    ui->pushButton_case->setVisible (!config.getHideSearchbar());
    ui->toolButton_nxt->setVisible (!config.getHideSearchbar());
    ui->toolButton_prv->setVisible (!config.getHideSearchbar());
    ui->pushButton_whole->setVisible (!config.getHideSearchbar());

    ui->actionDoc->setVisible (!config.getShowStatusbar());

    ui->actionWrap->setChecked (config.getWrapByDefault());

    ui->actionIndent->setChecked (config.getIndentByDefault());

    ui->actionLineNumbers->setChecked (config.getLineByDefault());
    ui->actionLineNumbers->setDisabled (config.getLineByDefault());

    ui->actionSyntax->setChecked (config.getSyntaxByDefault());
}
/*************************/
// Here leftIndx is the tab's index, to whose right all tabs are to be closed.
// Similarly, rightIndx is the tab's index, to whose left all tabs are to be closed.
// If they're both equal to -1, all tabs will be closed.
bool FPwin::closeTabs (int leftIndx, int rightIndx, bool closeall)
{
    bool keep = false;
    int index, count;
    int unsaved = 0;
    TextEdit *textEdit = NULL;
    Highlighter *highlighter = NULL;
    tabInfo *tabinfo = NULL;
    while (unsaved == 0 && ui->tabWidget->count() > 0)
    {
        if (rightIndx == 0) break; // no tab on the left
        if (rightIndx > -1)
            index = rightIndx - 1;
        else
            index = ui->tabWidget->count() - 1;

        if (index == leftIndx)
            break;
        else
            ui->tabWidget->setCurrentIndex (index);
        if (closeall && closeAll_)
            unsaved = 2;
        else if ((leftIndx > -1 && leftIndx == ui->tabWidget->count() - 2) || rightIndx == 1)
            unsaved = unSaved (index, false);
        else
            unsaved = unSaved (index, true);
        switch (unsaved) {
        case 0: // close this tab and go to the next one
            keep = false;
            textEdit = qobject_cast< TextEdit *>(ui->tabWidget->widget (index));
            disconnect (textEdit, &QPlainTextEdit::textChanged, this, &FPwin::wordButtonStatus);
            disconnect (textEdit, &QPlainTextEdit::textChanged, this, &FPwin::hlight);
            tabinfo = tabsInfo_[textEdit];
            highlighter = tabinfo->highlighter;
            delete highlighter; highlighter = NULL;
            tabsInfo_.remove (textEdit);
            delete tabinfo; tabinfo = NULL;
            ui->tabWidget->removeTab (index);
            delete textEdit; textEdit = NULL;

            if (rightIndx > -1) --rightIndx; // a left tab is removed

            /* final changes */
            count = ui->tabWidget->count();
            if (count == 0)
            {
                ui->actionReload->setDisabled (true);
                ui->actionSave->setDisabled (true);
                enableWidgets (false);
            }
            if (count == 1)
                ui->actionDetach->setDisabled (true);
            break;
        case 1: // stop quitting
            keep = true;
            break;
        case 2: // no to all: close all tabs (and quit)
            keep = false;
            closeAll_ = true; // needed only in closeOtherTabs()
            while (index > leftIndx)
            {
                if (rightIndx == 0) break;
                ui->tabWidget->setCurrentIndex (index);

                textEdit = qobject_cast< TextEdit *>(ui->tabWidget->widget (index));
                disconnect (textEdit, &QPlainTextEdit::textChanged, this, &FPwin::wordButtonStatus);
                disconnect (textEdit, &QPlainTextEdit::textChanged, this, &FPwin::hlight);
                tabinfo = tabsInfo_[textEdit];
                highlighter = tabinfo->highlighter;
                delete highlighter; highlighter = NULL;
                tabsInfo_.remove (textEdit);
                delete tabinfo; tabinfo = NULL;
                ui->tabWidget->removeTab (index);
                delete textEdit; textEdit = NULL;

                if (rightIndx > -1)
                {
                    --rightIndx; // a left tab is removed
                    index = rightIndx - 1;
                }
                else
                    index = ui->tabWidget->count() - 1;

                count = ui->tabWidget->count();
                if (count == 0)
                {
                    ui->actionReload->setDisabled (true);
                    ui->actionSave->setDisabled (true);
                    enableWidgets (false);
                }
                if (count == 1)
                    ui->actionDetach->setDisabled (true);
            }
            break;
        default:
            break;
        }
    }

    return keep;
}
/*************************/
void FPwin::closeAllTabs()
{
    closeAll_ = false; // may have been set to true
    closeTabs (-1, -1, true);
}
/*************************/
void FPwin::closeRightTabs()
{
    closeTabs (rightClicked_, -1, false);
}
/*************************/
void FPwin::closeLeftTabs()
{
    closeTabs (-1, rightClicked_, false);
}
/*************************/
void FPwin::closeOtherTabs()
{
    closeAll_ = false; // may have been set to true
    if (!closeTabs (rightClicked_, -1, false))
        closeTabs (-1, rightClicked_, true);
}
/*************************/
void FPwin::dragEnterEvent (QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
}
/*************************/
void FPwin::dropEvent (QDropEvent *event)
{
    foreach (QUrl url, event->mimeData()->urls())
        newTabFromName (url.toLocalFile());

    event->acceptProposedAction();
}
/*************************/
// This method checks if there's any text that isn't saved yet.
int FPwin::unSaved (int index, bool noToAll)
{
    int unsaved = 0;
    TextEdit *textEdit = qobject_cast< TextEdit *>(ui->tabWidget->widget (index));
    QString fname = tabsInfo_[textEdit]->fileName;
    if (textEdit->document()->isModified()
        || (!fname.isEmpty() && !QFile::exists (fname)))
    {
        QMessageBox msgBox (this);
        msgBox.setIcon (QMessageBox::Warning);
        msgBox.setText (tr ("<center><b><big>Save changes?</big></b></center>"));
        if (textEdit->document()->isModified())
            msgBox.setInformativeText (tr ("<center><i>The document has been modified.</i></center>"));
        else
            msgBox.setInformativeText (tr ("<center><i>The file has been removed.</i></center>"));
        if (noToAll && ui->tabWidget->count() > 1)
            msgBox.setStandardButtons (QMessageBox::Save
                                       | QMessageBox::Discard
                                       | QMessageBox::Cancel
                                       | QMessageBox::NoToAll);
        else
            msgBox.setStandardButtons (QMessageBox::Save
                                       | QMessageBox::Discard
                                       | QMessageBox::Cancel);
        msgBox.setButtonText (QMessageBox::Discard, tr ("Discard changes"));
        if (noToAll)
            msgBox.setButtonText (QMessageBox::NoToAll, tr ("No to all"));
        msgBox.setDefaultButton (QMessageBox::Save);
        msgBox.setWindowModality (Qt::WindowModal);
        msgBox.setWindowFlags (Qt::Dialog);
        /* enforce a central position (QtCurve bug?) */
        /*msgBox.show();
        msgBox.move (x() + width()/2 - msgBox.width()/2,
                     y() + height()/2 - msgBox.height()/ 2);*/
        switch (msgBox.exec()) {
        case QMessageBox::Save:
            if (!fileSave())
                unsaved = 1;
            break;
        case QMessageBox::Discard:
            break;
        case QMessageBox::Cancel:
            unsaved = 1;
            break;
        case QMessageBox::NoToAll:
            unsaved = 2;
            break;
        default:
            unsaved = 1;
            break;
        }
    }
    return unsaved;
}
/*************************/
// Enable or disable some widgets.
void FPwin::enableWidgets (bool enable) const
{
    Config config = static_cast<FPsingleton*>(qApp)->getConfig();
    if (!config.getHideSearchbar() || !enable)
    {
        ui->lineEdit->setVisible (enable);
        ui->pushButton_case->setVisible (enable);
        ui->toolButton_nxt->setVisible (enable);
        ui->toolButton_prv->setVisible (enable);
        ui->pushButton_whole->setVisible (enable);
    }
    if (!enable && ui->dockReplace->isVisible())
        ui->dockReplace->setVisible (false);
    if (!enable && ui->spinBox->isVisible())
    {
        ui->spinBox->setVisible (false);
        ui->label->setVisible (false);
        ui->checkBox->setVisible (false);
    }
    if ((!enable && ui->statusBar->isVisible())
        || (config.getShowStatusbar() && enable)) // starting from no tab
    {
        ui->statusBar->setVisible (enable);
    }

    ui->actionSelectAll->setEnabled (enable);
    ui->actionFind->setEnabled (enable);
    ui->actionJump->setEnabled (enable);
    ui->actionReplace->setEnabled (enable);
    ui->actionClose->setEnabled (enable);
    ui->actionSaveAs->setEnabled (enable);
    ui->menuEncoding->setEnabled (enable);
    ui->actionSaveCodec->setEnabled (enable);
    ui->actionFont->setEnabled (enable);
    if (!config.getShowStatusbar())
        ui->actionDoc->setEnabled (enable);
    ui->actionPrint->setEnabled (enable);

    if (!enable)
    {
        ui->actionUndo->setEnabled (false);
        ui->actionRedo->setEnabled (false);

        ui->actionEdit->setVisible (false);

        ui->actionCut->setEnabled (false);
        ui->actionCopy->setEnabled (false);
        ui->actionPaste->setEnabled (false);
        ui->actionDelete->setEnabled (false);
    }
}
/*************************/
void FPwin::newTab()
{
    Config config = static_cast<FPsingleton*>(qApp)->getConfig();
    TextEdit *textEdit = new TextEdit;
    textEdit->setScrollJumpWorkaround (config.getScrollJumpWorkaround());
    QPalette palette = QApplication::palette();
    QBrush brush = palette.window();
    if (brush.color().value() <= 120)
        textEdit->viewport()->setStyleSheet (".QWidget {"
                                             "color: black;"
                                             "background-color: rgb(236, 236, 236);}");
    /*else
        textEdit->viewport()->setStyleSheet (".QWidget {"
                                             "color: black;"
                                             "background-color: rgb(255, 255, 255);}");*/
    textEdit->document()->setDefaultFont (config.getFont());
    /* we want consistent tabs */
    QFontMetrics metrics (config.getFont());
    textEdit->setTabStopWidth (4 * metrics.width (' '));

    int index = ui->tabWidget->currentIndex();
    if (index == -1) enableWidgets (true);

    /* first make the preliminary info
       and only then add a new tab */
    tabInfo *tabinfo = new tabInfo;
    tabinfo->fileName = QString();
    tabinfo->size = 0;
    tabinfo->searchEntry = QString();
    tabinfo->encoding = "UTF-8";
    tabinfo->prog = QString();
    tabinfo->wordNumber = -1;
    tabinfo->highlighter = NULL;
    tabinfo->greenSel = QList<QTextEdit::ExtraSelection>();
    tabinfo->redSel = QList<QTextEdit::ExtraSelection>();
    tabsInfo_[textEdit] = tabinfo;

    ui->tabWidget->insertTab (index + 1, textEdit, "Untitled");
    /* when a user opens a tab, he wants to use it */
    ui->tabWidget->setCurrentWidget (textEdit);

    /* set all preliminary properties */
    if (index >= 0)
        ui->actionDetach->setEnabled (true);
    ui->tabWidget->setTabToolTip (index + 1, "Unsaved");
    if (!ui->actionWrap->isChecked())
        textEdit->setLineWrapMode (QPlainTextEdit::NoWrap);
    if (!ui->actionIndent->isChecked())
        textEdit->autoIndentation = false;
    if (ui->actionLineNumbers->isChecked() || ui->spinBox->isVisible())
        textEdit->showLineNumbers (true);
    if (ui->spinBox->isVisible())
        connect (textEdit->document(), &QTextDocument::blockCountChanged, this, &FPwin::setMax);
    if (ui->statusBar->isVisible()
        || config.getShowStatusbar()) // when the main window is being created, isVisible() isn't set yet
    {
        if (QToolButton *wordButton = ui->statusBar->findChild<QToolButton *>())
            wordButton->setVisible (false);
        QLabel *statusLabel = ui->statusBar->findChild<QLabel *>();
        statusLabel->setText (tr ("<b>Encoding:</b> <i>UTF-8</i>"
                                  "&nbsp;&nbsp;&nbsp;&nbsp;<b>Lines:</b> <i>1</i>"
                                  "&nbsp;&nbsp;&nbsp;&nbsp;<b>Sel. Chars.:</b> <i>0</i>"
                                  "&nbsp;&nbsp;&nbsp;&nbsp;<b>Words:</b> <i>0</i>"));

        connect (textEdit, &QPlainTextEdit::textChanged, this, &FPwin::wordButtonStatus);
        connect (textEdit, &QPlainTextEdit::blockCountChanged, this, &FPwin::statusMsgWithLineCount);
        connect (textEdit, &QPlainTextEdit::selectionChanged, this, &FPwin::statusMsg);
    }
    connect (textEdit->document(), &QTextDocument::undoAvailable, ui->actionUndo, &QAction::setEnabled);
    connect (textEdit->document(), &QTextDocument::redoAvailable, ui->actionRedo, &QAction::setEnabled);
    connect (textEdit->document(), &QTextDocument::modificationChanged, ui->actionSave, &QAction::setEnabled);
    connect (textEdit->document(), &QTextDocument::modificationChanged, this, &FPwin::asterisk);
    connect (textEdit, &QPlainTextEdit::copyAvailable, ui->actionCut, &QAction::setEnabled);
    connect (textEdit, &QPlainTextEdit::copyAvailable, ui->actionDelete, &QAction::setEnabled);
    connect (textEdit, &QPlainTextEdit::copyAvailable, ui->actionCopy, &QAction::setEnabled);
    connect (textEdit, &TextEdit::fileDropped, this, &FPwin::newTabFromName);
    connect (textEdit, &QPlainTextEdit::cursorPositionChanged, this, &FPwin::matchBrackets);

    /* I don't know why, under KDE, when text is selected
       for the first time, it isn't copied to the selection
       clipboard. Perhaps it has something to do with Klipper.
       I neither know why this s a workaround: */
    QApplication::clipboard()->text (QClipboard::Selection);

    textEdit->setFocus();

    /* this isn't enough for unshading under all WMs */
    /*if (isMinimized())
        setWindowState (windowState() & (~Qt::WindowMinimized | Qt::WindowActive));*/
    if (isWindowShaded (winId()))
        unshadeWindow (winId());
    activateWindow();
    raise();
}
/*************************/
void FPwin::zoomIn()
{
    int index = ui->tabWidget->currentIndex();
    if (index == -1) return;

    TextEdit *textEdit = qobject_cast< TextEdit *>(ui->tabWidget->widget (index));
    QFont currentFont = textEdit->document()->defaultFont();
    int size = currentFont.pointSize();
    currentFont.setPointSize (size + 1);
    textEdit->document()->setDefaultFont (currentFont);
    QFontMetrics metrics (currentFont);
    textEdit->setTabStopWidth (4 * metrics.width (' '));

    /* this trick is sometimes needed for the
       scrollbar range(s) to be updated (a bug?) */
    QSize currSize = textEdit->size();
    QSize newSize;
    newSize.setWidth (currSize.width() + 1);
    newSize.setHeight (currSize.height() + 1);
    textEdit->resize (newSize);
    textEdit->resize (currSize);
}
/*************************/
void FPwin::zoomOut()
{
    int index = ui->tabWidget->currentIndex();
    if (index == -1) return;

    TextEdit *textEdit = qobject_cast< TextEdit *>(ui->tabWidget->widget (index));
    QFont currentFont = textEdit->document()->defaultFont();
    int size = currentFont.pointSize();
    if (size <= 3) return;
    currentFont.setPointSize (size - 1);
    textEdit->document()->setDefaultFont (currentFont);
    QFontMetrics metrics (currentFont);
    textEdit->setTabStopWidth (4 * metrics.width (' '));

    if (!tabsInfo_[textEdit]->searchEntry.isEmpty())
        hlight();
}
/*************************/
void FPwin::zoomZero()
{
    int index = ui->tabWidget->currentIndex();
    if (index == -1) return;

    Config config = static_cast<FPsingleton*>(qApp)->getConfig();
    TextEdit *textEdit = qobject_cast< TextEdit *>(ui->tabWidget->widget (index));
    textEdit->document()->setDefaultFont (config.getFont());
    QFontMetrics metrics (config.getFont());
    textEdit->setTabStopWidth (4 * metrics.width (' '));

    QSize currSize = textEdit->size();
    QSize newSize;
    newSize.setWidth (currSize.width() + 1);
    newSize.setHeight (currSize.height() + 1);
    textEdit->resize (newSize);
    textEdit->resize (currSize);

    /* this may be a zoom-out */
    if (!tabsInfo_[textEdit]->searchEntry.isEmpty())
        hlight();
}
/*************************/
void FPwin::fullScreening()
{
    setWindowState (windowState() ^ Qt::WindowFullScreen);
}
/*************************/
void FPwin::defaultSize()
{
    if (size() == QSize (700, 500)) return;
    if (isMaximized() && isFullScreen())
        showMaximized();
    if (isMaximized())
        showNormal();
    /* instead of hiding, reparent with the dummy
       widget to guarantee resizing under all DEs */
    setParent (dummyWidget, Qt::SubWindow);
    resize (700, 500);
    if (parent() != NULL)
        setParent (NULL, Qt::Window);
    QTimer::singleShot (0, this, SLOT (show()));
}
/*************************/
void FPwin::align()
{
    int index = ui->tabWidget->currentIndex();
    if (index == -1) return;

    TextEdit *textEdit = qobject_cast< TextEdit *>(ui->tabWidget->widget (index));
    QTextOption opt = textEdit->document()->defaultTextOption();
    if (opt.alignment() == (Qt::AlignLeft))
    {
        opt = QTextOption (Qt::AlignRight);
        opt.setTextDirection (Qt::LayoutDirectionAuto);
        textEdit->document()->setDefaultTextOption (opt);
    }
    else if (opt.alignment() == (Qt::AlignRight))
    {
        opt = QTextOption (Qt::AlignLeft);
        opt.setTextDirection (Qt::LayoutDirectionAuto);
        textEdit->document()->setDefaultTextOption (opt);
    }
}
/*************************/
void FPwin::closeTab()
{
    int index = ui->tabWidget->currentIndex();
    if (index == -1) return; // not needed

    if (unSaved (index, false)) return;

    /* Because deleting the syntax highlighter later will change
       the text, we have two options for preventing a possible crash:
       (1) disconnecting the textChanged() signals now; or
       (2) truncating searchEntries after deleting the syntax highlighter. */
    TextEdit *textEdit = qobject_cast< TextEdit *>(ui->tabWidget->widget (index));
    disconnect (textEdit, &QPlainTextEdit::textChanged, this, &FPwin::wordButtonStatus);
    disconnect (textEdit, &QPlainTextEdit::textChanged, this, &FPwin::hlight);
    tabInfo *tabinfo = tabsInfo_[textEdit];
    Highlighter *highlighter = tabinfo->highlighter;
    delete highlighter; highlighter = NULL;
    tabsInfo_.remove (textEdit);
    delete tabinfo; tabinfo = NULL;
    ui->tabWidget->removeTab (index);
    delete textEdit; textEdit = NULL;
    int count = ui->tabWidget->count();
    if (count == 0)
    {
        ui->actionReload->setDisabled (true);
        ui->actionSave->setDisabled (true);
        enableWidgets (false);
    }
    else if (count == 1)
        ui->actionDetach->setDisabled (true);
}
/*************************/
void FPwin::closeTabAtIndex (int index)
{
    if (unSaved (index, false)) return;

    TextEdit *textEdit = qobject_cast< TextEdit *>(ui->tabWidget->widget (index));
    disconnect (textEdit, &QPlainTextEdit::textChanged, this, &FPwin::wordButtonStatus);
    disconnect (textEdit, &QPlainTextEdit::textChanged, this, &FPwin::hlight);
    tabInfo *tabinfo = tabsInfo_[textEdit];
    Highlighter *highlighter = tabinfo->highlighter;
    delete highlighter; highlighter = NULL;
    tabsInfo_.remove (textEdit);
    delete tabinfo; tabinfo = NULL;
    ui->tabWidget->removeTab (index);
    delete textEdit; textEdit = NULL;
    int count = ui->tabWidget->count();
    if (count == 0)
    {
        ui->actionReload->setDisabled (true);
        ui->actionSave->setDisabled (true);
        enableWidgets (false);
    }
    else if (count == 1)
        ui->actionDetach->setDisabled (true);
}
/*************************/
void FPwin::setTitle (const QString& fileName)
{
    int index = ui->tabWidget->currentIndex();

    QString shownName;
    if (fileName.isEmpty())
        shownName = tr ("Untitled");
    else
        shownName = QFileInfo (fileName).fileName();

    ui->tabWidget->setTabText (index, shownName);
    setWindowTitle (shownName);
}
/*************************/
void FPwin::asterisk (bool modified)
{
    int index = ui->tabWidget->currentIndex();

    QString fname = tabsInfo_[qobject_cast< TextEdit *>(ui->tabWidget->widget (index))]
                    ->fileName;
    QString shownName;
    if (modified)
    {
        if (fname.isEmpty())
            shownName = tr ("*Untitled");
        else
            shownName = QFileInfo (fname).fileName().prepend("*");
    }
    else
    {
        if (fname.isEmpty())
            shownName = tr ("Untitled");
        else
            shownName = QFileInfo (fname).fileName();
    }

    ui->tabWidget->setTabText (index, shownName);
    setWindowTitle (shownName);
}
/*************************/
void FPwin::loadText (const QString& fileName, bool enforceEncod, bool reload)
{
    QString charset = "";
    if (enforceEncod)
        charset = checkToEncoding();
    Loading *thread = new Loading (fileName, charset, enforceEncod, reload);
    connect (thread, &Loading::completed, this, &FPwin::addText);
    connect (thread, &Loading::finished, thread, &QObject::deleteLater);
    thread->start();
}
/*************************/
void FPwin::addText (const QString text, const QString fileName, const QString charset,
                     bool enforceEncod, bool reload)
{
    if (ui->tabWidget->count() == 0)
        newTab();
    TextEdit *textEdit = qobject_cast< TextEdit *>(ui->tabWidget->currentWidget());
    if (!reload
        && !enforceEncod
        && (!textEdit->document()->isEmpty()
            || textEdit->document()->isModified()
            || !tabsInfo_[textEdit]->fileName.isEmpty()))
    {
        newTab();
        textEdit = qobject_cast< TextEdit *>(ui->tabWidget->currentWidget());
    }
    else
    {
        /*if (isMinimized())
            setWindowState (windowState() & (~Qt::WindowMinimized | Qt::WindowActive));*/
        if (isWindowShaded (winId()))
            unshadeWindow (winId());
        activateWindow();
        raise();
    }

    /* this is a workaround for the RTL bug in QPlainTextEdit */
    QTextOption opt = textEdit->document()->defaultTextOption();
    if (text.isRightToLeft())
    {
        if (opt.alignment() == (Qt::AlignLeft))
        {
            opt = QTextOption (Qt::AlignRight);
            opt.setTextDirection (Qt::LayoutDirectionAuto);
            textEdit->document()->setDefaultTextOption (opt);
        }
    }
    else if (opt.alignment() == (Qt::AlignRight))
    {
        opt = QTextOption (Qt::AlignLeft);
        opt.setTextDirection (Qt::LayoutDirectionAuto);
        textEdit->document()->setDefaultTextOption (opt);
    }

    /* we want to restore the cursor later */
    int pos = 0, anchor = 0;
    if (reload)
    {
        pos = textEdit->textCursor().position();
        anchor = textEdit->textCursor().anchor();
    }

    /* set the text */
    disconnect (textEdit->document(), &QTextDocument::modificationChanged, ui->actionSave, &QAction::setEnabled);
    disconnect (textEdit->document(), &QTextDocument::modificationChanged, this, &FPwin::asterisk);
    textEdit->setPlainText (text);
    connect (textEdit->document(), &QTextDocument::modificationChanged, ui->actionSave, &QAction::setEnabled);
    connect (textEdit->document(), &QTextDocument::modificationChanged, this, &FPwin::asterisk);

    /* now, restore the cursor */
    if (reload)
    {
        QTextCursor cur = textEdit->textCursor();
        cur.movePosition (QTextCursor::End, QTextCursor::MoveAnchor);
        int curPos = cur.position();
        if (anchor <= curPos && pos <= curPos)
        {
            cur.setPosition (anchor);
            cur.setPosition (pos, QTextCursor::KeepAnchor);
        }
        textEdit->setTextCursor (cur);
    }

    int index = ui->tabWidget->currentIndex();
    tabInfo *tabinfo = tabsInfo_[textEdit];
    if (enforceEncod || reload)
    {
        tabinfo->greenSel = QList<QTextEdit::ExtraSelection>();

        Highlighter *highlighter = tabinfo->highlighter;
        tabinfo->highlighter = NULL;
        delete highlighter; highlighter = NULL;
    }

    QFileInfo fInfo = (fileName);

    tabinfo->fileName = fileName;
    tabinfo->size = fInfo.size();
    lastFile_ = fileName;
    tabinfo->encoding = charset;
    tabinfo->wordNumber = -1;
    getSyntax (index);
    if (ui->statusBar->isVisible())
    {
        statusMsgWithLineCount (textEdit->document()->blockCount());
        if (QToolButton *wordButton = ui->statusBar->findChild<QToolButton *>())
            wordButton->setVisible (true);
    }
    if (ui->actionSyntax->isChecked()
        && tabinfo->size <= static_cast<FPsingleton*>(qApp)->getConfig().getMaxSHSize()*1024*1024)
    {
        syntaxHighlighting (index);
    }

    encodingToCheck (charset);
    ui->actionReload->setEnabled (true);
    setTitle (fileName);
    QString tip (fInfo.absolutePath() + "/");
    QFontMetrics metrics (QToolTip::font());
    int w = QApplication::desktop()->screenGeometry().width();
    if (w > 200 * metrics.width (' ')) w = 200 * metrics.width (' ');
    QString elidedTip = metrics.elidedText (tip, Qt::ElideMiddle, w);
    ui->tabWidget->setTabToolTip (index, elidedTip);

    if (alreadyOpen (fileName))
    {
        textEdit->setReadOnly (true);
        textEdit->viewport()->setStyleSheet (".QWidget {"
                                             "color: black;"
                                             "background-color: rgb(236, 236, 208);}");
        ui->actionEdit->setVisible (true);
        ui->actionCut->setDisabled (true);
        ui->actionPaste->setDisabled (true);
        ui->actionDelete->setDisabled (true);
        disconnect (textEdit, &QPlainTextEdit::copyAvailable, ui->actionCut, &QAction::setEnabled);
        disconnect (textEdit, &QPlainTextEdit::copyAvailable, ui->actionDelete, &QAction::setEnabled);
    }
    else if (textEdit->isReadOnly())
        makeEditable();
}

/*************************/
void FPwin::newTabFromName (const QString& fileName)
{
    if (!fileName.isEmpty())
        loadText (fileName, false, false);
}
/*************************/
void FPwin::fileOpen()
{
    /* find a suitable directory */
    int index = ui->tabWidget->currentIndex();
    QString fname;
    if (index > -1)
        fname = tabsInfo_[qobject_cast< TextEdit *>(ui->tabWidget->widget (index))]
                ->fileName;

    QString path;
    if (!fname.isEmpty())
    {
        if (QFile::exists (fname))
            path = fname;
        else
        {
            QDir dir = QFileInfo (fname).absoluteDir();
            if (!dir.exists())
                dir = QDir::home();
            path = dir.path();
        }
    }
    else
    {
        /* I like the last opened file to be remembered */
        fname = lastFile_;
        if (!fname.isEmpty())
        {
            QDir dir = QFileInfo (fname).absoluteDir();
            if (!dir.exists())
                dir = QDir::home();
            path = dir.path();
        }
        else
        {
            QDir dir = QDir::home();
            path = dir.path();
        }
    }

    FileDialog dialog(this);
    dialog.setAcceptMode (QFileDialog::AcceptOpen);
    dialog.setWindowTitle (tr ("Open file..."));
    dialog.setFileMode (QFileDialog::ExistingFile);
    if (QFileInfo (path).isDir())
        dialog.setDirectory (path);
    else
    {
        dialog.selectFile (path);
        dialog.autoScroll();
    }
    if (dialog.exec())
    {
        fname = dialog.selectedFiles().at (0);
        newTabFromName (fname);
    }
}
/*************************/
// Check if the file is already opened for editing somewhere else.
bool FPwin::alreadyOpen (const QString& fileName) const
{
    bool res = false;
    TextEdit *textEdit = qobject_cast< TextEdit *>(ui->tabWidget->currentWidget());
    FPsingleton *singleton = static_cast<FPsingleton*>(qApp);
    for (int i = 0; i < singleton->Wins.count(); ++i)
    {
        FPwin *thisOne = singleton->Wins.at (i);
        QHash<TextEdit*,tabInfo*>::iterator it;
        for (it = thisOne->tabsInfo_.begin(); it != thisOne->tabsInfo_.end(); ++it)
        {
            if (thisOne == this && it.key() == textEdit)
                continue;
            if (thisOne->tabsInfo_[it.key()]->fileName == fileName &&
                !it.key()->isReadOnly())
            {
                res = true;
                break;
            }
        }
        if (res) break;
    }
    return res;
}
/*************************/
void FPwin::enforceEncoding (QAction*)
{
    int index = ui->tabWidget->currentIndex();
    if (index == -1) return;

    TextEdit *textEdit = qobject_cast< TextEdit *>(ui->tabWidget->widget (index));
    tabInfo *tabinfo = tabsInfo_[textEdit];
    QString fname = tabinfo->fileName;
    if (!fname.isEmpty())
    {
        if (unSaved (index, false))
        {
            /* back to the previous encoding */
            encodingToCheck (tabinfo->encoding);
            return;
        }
        loadText (fname, true, true);
    }
    else
    {
        tabinfo->encoding = checkToEncoding();
        if (ui->statusBar->isVisible())
        {
            QLabel *statusLabel = ui->statusBar->findChild<QLabel *>();
            QString str = statusLabel->text();
            int i = str.indexOf ("Encoding");
            int j = str.indexOf ("</i>&nbsp;&nbsp;&nbsp;&nbsp;<b>Lines");
            str.replace (i + 17, j - i - 17, checkToEncoding());
            statusLabel->setText (str);
        }
    }
}
/*************************/
void FPwin::reload()
{
    int index = ui->tabWidget->currentIndex();
    if (index == -1) return;

    if (unSaved (index, false)) return;

    QString fname = tabsInfo_[qobject_cast< TextEdit *>(ui->tabWidget->widget (index))]
                    ->fileName;
    if (!fname.isEmpty()) loadText (fname, false, true);
}
/*************************/
// This is for both "Save" and "Save As"
bool FPwin::fileSave()
{
    int index = ui->tabWidget->currentIndex();
    if (index == -1) return false;

    TextEdit *textEdit = qobject_cast< TextEdit *>(ui->tabWidget->widget (index));
    tabInfo *tabinfo = tabsInfo_[textEdit];
    QString fname = tabinfo->fileName;
    if (fname.isEmpty()) fname = lastFile_;
    QString filter;
    if (!fname.isEmpty()
        && QFileInfo (fname).fileName().contains ('.'))
    {
        /* if relevant, do filtering to prevent disastrous overwritings */
        filter = tr (".%1 Files(*.%1);;All Files(*)").arg (fname.section ('.', -1, -1));
    }

    if (fname.isEmpty()
        || !QFile::exists (fname)
        || tabinfo->fileName.isEmpty())
    {
        bool restorable = false;
        if (fname.isEmpty())
        {
            QDir dir = QDir::home();
            fname = dir.filePath ("Untitled");
        }
        else if (!QFile::exists (fname))
        {
            QDir dir = QFileInfo (fname).absoluteDir();
            if (!dir.exists())
            {
                dir = QDir::home();
                if (tabinfo->fileName.isEmpty())
                    filter = QString();
            }
            /* if the removed file is opened in this tab and its
               containing folder still exists, it's restorable */
            else if (!tabinfo->fileName.isEmpty())
                restorable = true;
            /* add the file name */
            if (restorable)
                fname = dir.filePath (QFileInfo (fname).fileName());
            else
                fname = dir.filePath ("Untitled");
        }
        else
            fname = QFileInfo (fname).absoluteDir().filePath ("Untitled");

        /* use Save-As for Save or saving */
        if (!restorable
            && QObject::sender() != ui->actionSaveAs
            && QObject::sender() != ui->actionSaveCodec)
        {
            FileDialog dialog (this);
            dialog.setAcceptMode (QFileDialog::AcceptSave);
            dialog.setWindowTitle (tr ("Save as..."));
            dialog.setFileMode (QFileDialog::AnyFile);
            dialog.setNameFilter (filter);
            dialog.selectFile (fname);
            dialog.autoScroll();
            if (dialog.exec())
            {
                fname = dialog.selectedFiles().at (0);
                if (fname.isEmpty() || QFileInfo (fname).isDir())
                    return false;
            }
            else
                return false;
        }
    }

    if (QObject::sender() == ui->actionSaveAs)
    {
        FileDialog dialog (this);
        dialog.setAcceptMode (QFileDialog::AcceptSave);
        dialog.setWindowTitle (tr ("Save as..."));
        dialog.setFileMode (QFileDialog::AnyFile);
        dialog.setNameFilter (filter);
        dialog.selectFile (fname);
        dialog.autoScroll();
        if (dialog.exec())
        {
            fname = dialog.selectedFiles().at (0);
            if (fname.isEmpty() || QFileInfo (fname).isDir())
                return false;
        }
        else
            return false;
    }
    else if (QObject::sender() == ui->actionSaveCodec)
    {
        FileDialog dialog (this);
        dialog.setAcceptMode (QFileDialog::AcceptSave);
        dialog.setWindowTitle (tr ("Keep encoding and save as..."));
        dialog.setFileMode (QFileDialog::AnyFile);
        dialog.setNameFilter (filter);
        dialog.selectFile (fname);
        dialog.autoScroll();
        if (dialog.exec())
        {
            fname = dialog.selectedFiles().at (0);
            if (fname.isEmpty() || QFileInfo (fname).isDir())
                return false;
        }
        else
            return false;
    }

    /* try to write */
    QTextDocumentWriter writer (fname, "plaintext");
    bool success = false;
    if (QObject::sender() == ui->actionSaveCodec)
    {
        QString encoding  = checkToEncoding();

        QMessageBox msgBox (this);
        msgBox.setIcon (QMessageBox::Question);
        msgBox.addButton (QMessageBox::Yes);
        msgBox.addButton (QMessageBox::No);
        msgBox.addButton (QMessageBox::Cancel);
        msgBox.setText (tr ("<center>Do you want to use <b>MS Windows</b> end-of-lines?</center>"));
        msgBox.setInformativeText (tr ("<center><i>This may be good for readability under MS Windows.</i></center>"));
        msgBox.setWindowModality (Qt::WindowModal);
        msgBox.setWindowFlags (Qt::Dialog);
        QString contents;
        int ln;
        QTextCodec *codec;
        QByteArray encodedString;
        const char *txt;
        switch (msgBox.exec()) {
        case QMessageBox::Yes:
            contents = textEdit->document()->toPlainText();
            contents.replace ("\n", "\r\n");
            ln = contents.length(); // for fwrite();
            codec = QTextCodec::codecForName (checkToEncoding().toUtf8());
            encodedString = codec->fromUnicode (contents);
            txt = encodedString.constData();
            if (encoding != "UTF-16")
            {
                std::ofstream file;
                file.open (fname.toUtf8().constData());
                if (file.is_open())
                {
                    file << txt;
                    file.close();
                    success = true;
                }
            }
            else
            {
                FILE * file;
                file = fopen (fname.toUtf8().constData(), "wb");
                if (file != NULL)
                {
                    /* this worked correctly as I far as I tested */
                    fwrite (txt , 2 , ln + 1 , file);
                    fclose (file);
                    success = true;
                }
            }
            break;
        case QMessageBox::No:
            writer.setCodec (QTextCodec::codecForName (encoding.toUtf8()));
            break;
        default:
            return false;
            break;
        }
    }
    if (!success)
        success = writer.write (textEdit->document());

    if (success)
    {
        QFileInfo fInfo = (fname);

        textEdit->document()->setModified (false);
        tabinfo->fileName = fname;
        tabinfo->size = fInfo.size();
        ui->actionReload->setDisabled (false);
        setTitle (fname);
        QString tip (fInfo.absolutePath() + "/");
        QFontMetrics metrics (QToolTip::font());
        int w = QApplication::desktop()->screenGeometry().width();
        if (w > 200 * metrics.width (' ')) w = 200 * metrics.width (' ');
        QString elidedTip = metrics.elidedText (tip, Qt::ElideMiddle, w);
        ui->tabWidget->setTabToolTip (index, elidedTip);
        lastFile_ = fname;
    }
    else
    {
        QString str = writer.device()->errorString ();
        QMessageBox msgBox (QMessageBox::Warning,
                            tr ("FeatherPad"),
                            tr ("<center><b><big>Cannot be saved!</big></b></center>"),
                            QMessageBox::Close,
                            this);
        msgBox.setInformativeText (tr ("<center><i>%1.</i></center>").arg (str));
        msgBox.setWindowModality (Qt::WindowModal);
        msgBox.setWindowFlags (Qt::Dialog);
        msgBox.exec();
    }
    return success;
}
/*************************/
void FPwin::cutText()
{
    int index = ui->tabWidget->currentIndex();
    if (index == -1) return;

    qobject_cast< TextEdit *>(ui->tabWidget->widget (index))->cut();
}
/*************************/
void FPwin::copyText()
{
    int index = ui->tabWidget->currentIndex();
    if (index == -1) return;

    qobject_cast< TextEdit *>(ui->tabWidget->widget (index))->copy();
}
/*************************/
void FPwin::pasteText()
{
    int index = ui->tabWidget->currentIndex();
    if (index == -1) return;

    qobject_cast< TextEdit *>(ui->tabWidget->widget (index))->paste();
}
/*************************/
void FPwin::deleteText()
{
    int index = ui->tabWidget->currentIndex();
    if (index == -1) return;

    TextEdit *textEdit = qobject_cast< TextEdit *>(ui->tabWidget->widget (index));
    if (!textEdit->isReadOnly())
        textEdit->insertPlainText ("");
}
/*************************/
void FPwin::selectAllText()
{
    qobject_cast< TextEdit *>(ui->tabWidget->currentWidget())->selectAll();
}
/*************************/
void FPwin::makeEditable()
{
    int index = ui->tabWidget->currentIndex();
    if (index == -1) return;

    TextEdit *textEdit = qobject_cast< TextEdit *>(ui->tabWidget->widget (index));
    bool textIsSelected = textEdit->textCursor().hasSelection();

    textEdit->setReadOnly (false);
    QPalette palette = QApplication::palette();
    QBrush brush = palette.window();
    if (brush.color().value() <= 120)
        textEdit->viewport()->setStyleSheet (".QWidget {"
                                             "color: black;"
                                             "background-color: rgb(236, 236, 236);}");
    else
        textEdit->viewport()->setStyleSheet (".QWidget {"
                                             "color: black;"
                                             "background-color: rgb(255, 255, 255);}");
    ui->actionEdit->setVisible (false);

    ui->actionPaste->setEnabled (true);
    ui->actionCopy->setEnabled (textIsSelected);
    ui->actionCut->setEnabled (textIsSelected);
    ui->actionDelete->setEnabled (textIsSelected);
    connect (textEdit, &QPlainTextEdit::copyAvailable, ui->actionCut, &QAction::setEnabled);
    connect (textEdit, &QPlainTextEdit::copyAvailable, ui->actionDelete, &QAction::setEnabled);
}
/*************************/
void FPwin::undoing()
{
    int index = ui->tabWidget->currentIndex();
    if (index == -1) return;

    TextEdit *textEdit = qobject_cast< TextEdit *>(ui->tabWidget->widget (index));
    tabInfo *tabinfo = tabsInfo_[textEdit];

    /* remove green highlights */
    tabinfo->greenSel = QList<QTextEdit::ExtraSelection>();
    if (tabinfo->searchEntry.isEmpty())
    {
        QList<QTextEdit::ExtraSelection> extraSelections;
        if (ui->actionLineNumbers->isChecked() || ui->spinBox->isVisible())
            extraSelections.prepend (textEdit->currentLine);
        extraSelections.append (tabinfo->redSel);
        textEdit->setExtraSelections (extraSelections);
    }

    textEdit->undo();
}
/*************************/
void FPwin::redoing()
{
    int index = ui->tabWidget->currentIndex();
    if (index == -1) return;

    qobject_cast< TextEdit *>(ui->tabWidget->widget (index))->redo();
}
/*************************/
// Change the window title and the
// search entry when switching tabs and...
void FPwin::tabSwitch (int index)
{
    if (index == -1)
    {
        setWindowTitle (tr("%1[*]").arg ("FeatherPad"));
        setWindowModified (false);
        return;
    }

    QString shownName = ui->tabWidget->tabText (index);
    setWindowTitle (shownName);

    TextEdit *textEdit = qobject_cast< TextEdit *>(ui->tabWidget->widget (index));
    tabInfo *tabinfo = tabsInfo_[textEdit];

    /* also change the search entry */
    QString txt = tabinfo->searchEntry;
    ui->lineEdit->setText (txt);
    /* the window size or wrapping state may have changed
       or the replace dock may have been closed or
       or the replacing text may have changed */
    if (!txt.isEmpty()) hlight();

    /* correct the encoding menu */
    encodingToCheck (tabinfo->encoding);

    /* correct the states of some buttons */
    ui->actionUndo->setEnabled (textEdit->document()->isUndoAvailable());
    ui->actionRedo->setEnabled (textEdit->document()->isRedoAvailable());
    ui->actionSave->setEnabled (textEdit->document()->isModified());
    QString fileName = tabinfo->fileName;
    if (fileName.isEmpty())
        ui->actionReload->setEnabled (false);
    else
        ui->actionReload->setEnabled (true);
    bool readOnly = textEdit->isReadOnly();
    if (fileName.isEmpty()
        && !textEdit->document()->isModified()
        && !textEdit->document()->isEmpty()) // 'Help' is the exception
    {
        ui->actionEdit->setVisible (false);
    }
    else
    {
        ui->actionEdit->setVisible (readOnly);
    }
    ui->actionPaste->setEnabled (!readOnly);
    bool textIsSelected = textEdit->textCursor().hasSelection();
    ui->actionCopy->setEnabled (textIsSelected);
    ui->actionCut->setEnabled (!readOnly & textIsSelected);
    ui->actionDelete->setEnabled (!readOnly & textIsSelected);

    /* handle the spinbox */
    if (ui->spinBox->isVisible())
        ui->spinBox->setMaximum (textEdit->document()->blockCount());

    /* handle the statusbar */
    if (ui->statusBar->isVisible())
    {
        statusMsgWithLineCount (textEdit->document()->blockCount());
        QToolButton *wordButton = ui->statusBar->findChild<QToolButton *>();
        if (tabinfo->wordNumber == -1)
        {
            if (wordButton)
                wordButton->setVisible (true);
            if (textEdit->document()->isEmpty()) // make an exception
                wordButtonStatus();
        }
        else
        {
            if (wordButton)
                wordButton->setVisible (false);
            QLabel *statusLabel = ui->statusBar->findChild<QLabel *>();
            statusLabel->setText (tr ("%1 <i>%2</i>")
                                  .arg (statusLabel->text())
                                  .arg (tabinfo->wordNumber));
        }
    }
}
/*************************/
void FPwin::fontDialog()
{
    int index = ui->tabWidget->currentIndex();
    if (index == -1) return;

    TextEdit *textEdit = qobject_cast< TextEdit *>(ui->tabWidget->widget (index));

    QFont currentFont = textEdit->document()->defaultFont();
    QFontDialog fd (currentFont, this);
    fd.setOption (QFontDialog::DontUseNativeDialog);
    fd.setWindowModality (Qt::WindowModal);
    fd.move (this->x() + this->width()/2 - fd.width()/2,
             this->y() + this->height()/2 - fd.height()/ 2);
    if (fd.exec())
    {
        QFont newFont = fd.selectedFont();
        FPsingleton *singleton = static_cast<FPsingleton*>(qApp);
        for (int i = 0; i < singleton->Wins.count(); ++i)
        {
            FPwin *thisWin = singleton->Wins.at (i);
            QHash<TextEdit*,tabInfo*>::iterator it;
            for (it = thisWin->tabsInfo_.begin(); it != thisWin->tabsInfo_.end(); ++it)
            {
                it.key()->document()->setDefaultFont (newFont);
                QFontMetrics metrics (newFont);
                it.key()->setTabStopWidth (4 * metrics.width (' '));
            }
        }

        Config& config = static_cast<FPsingleton*>(qApp)->getConfig();
        if (config.getRemFont())
            config.setFont (newFont);

        if (!tabsInfo_[textEdit]->searchEntry.isEmpty())
            hlight();
    }
}
/*************************/
// Update highlights and get the new window size on resizing.
void FPwin::resizeEvent (QResizeEvent *event)
{
    Q_UNUSED (event);
    int index = ui->tabWidget->currentIndex();
    if (index > -1
        && !tabsInfo_[qobject_cast< TextEdit *>(ui->tabWidget->widget (index))]->searchEntry.isEmpty())
    {
        hlight();
    }
}
/*************************/
void FPwin::changeEvent (QEvent *event)
{
    Config& config = static_cast<FPsingleton*>(qApp)->getConfig();
    if (config.getRemSize() && event->type() == QEvent::WindowStateChange)
    {
        if (windowState() == Qt::WindowFullScreen)
        {
            config.setIsFull (true);
            config.setIsMaxed (false);
        }
        else if (windowState() == (Qt::WindowFullScreen ^ Qt::WindowMaximized))
        {
            config.setIsFull (true);
            config.setIsMaxed (true);
        }
        else
        {
            config.setIsFull (false);
            if (windowState() == Qt::WindowMaximized)
                config.setIsMaxed (true);
            else
                config.setIsMaxed (false);
        }
    }
    QWidget::changeEvent (event);
}
/*************************/
void FPwin::jumpTo()
{
    bool visibility = ui->spinBox->isVisible();

    QHash<TextEdit*,tabInfo*>::iterator it;
    for (it = tabsInfo_.begin(); it != tabsInfo_.end(); ++it)
    {
        if (!ui->actionLineNumbers->isChecked())
            it.key()->showLineNumbers (!visibility);

        if (!visibility)
        {
            /* setMaximum() isn't a slot */
            connect (it.key()->document(),
                     &QTextDocument::blockCountChanged,
                     this,
                     &FPwin::setMax);
        }
        else
            disconnect (it.key()->document(),
                        &QTextDocument::blockCountChanged,
                        this,
                        &FPwin::setMax);
    }

    if (!visibility && ui->tabWidget->count() > 0)
        ui->spinBox->setMaximum (qobject_cast< TextEdit *>(ui->tabWidget->currentWidget())
                                 ->document()
                                 ->blockCount());
    ui->spinBox->setVisible (!visibility);
    ui->label->setVisible (!visibility);
    ui->checkBox->setVisible (!visibility);
    if (!visibility)
    {
        ui->spinBox->setFocus();
        ui->spinBox->selectAll();
    }
    else
        /* return focus to doc */
        qobject_cast< TextEdit *>(ui->tabWidget->currentWidget())->setFocus();
}
/*************************/
void FPwin::setMax (const int max)
{
    ui->spinBox->setMaximum (max);
}
/*************************/
void FPwin::goTo()
{
    /* workaround for not being able to use returnPressed()
       because of protectedness of spinbox's QLineEdit */
    if (!ui->spinBox->hasFocus()) return;

    TextEdit *textEdit = qobject_cast< TextEdit *>(ui->tabWidget->currentWidget());
    QTextBlock block = textEdit->document()->findBlockByNumber (ui->spinBox->value() - 1);
    int pos = block.position();
    QTextCursor start = textEdit->textCursor();
    if (ui->checkBox->isChecked())
        start.setPosition (pos, QTextCursor::KeepAnchor);
    else
        start.setPosition (pos);
    textEdit->setTextCursor (start);
}
/*************************/
void FPwin::showLN (bool checked)
{
    int count = ui->tabWidget->count();
    if (count == 0) return;

    if (checked)
    {
        for (int i = 0; i < count; ++i)
            qobject_cast< TextEdit *>(ui->tabWidget->widget (i))->showLineNumbers (true);
    }
    else if (!ui->spinBox->isVisible()) // also the spinBox affects line numbers visibility
    {
        for (int i = 0; i < count; ++i)
            qobject_cast< TextEdit *>(ui->tabWidget->widget (i))->showLineNumbers (false);
    }
}
/*************************/
void FPwin::toggleWrapping()
{
    int count = ui->tabWidget->count();
    if (count == 0) return;

    if (ui->actionWrap->isChecked())
    {
        for (int i = 0; i < count; ++i)
            qobject_cast< TextEdit *>(ui->tabWidget->widget (i))->setLineWrapMode (QPlainTextEdit::WidgetWidth);
    }
    else
    {
        QHash<TextEdit*,tabInfo*>::iterator it;
        for (it = tabsInfo_.begin(); it != tabsInfo_.end(); ++it)
            it.key()->setLineWrapMode (QPlainTextEdit::NoWrap);
    }
    hlight();
}
/*************************/
void FPwin::toggleIndent()
{
    int count = ui->tabWidget->count();
    if (count == 0) return;

    if (ui->actionIndent->isChecked())
    {
        QHash<TextEdit*,tabInfo*>::iterator it;
        for (it = tabsInfo_.begin(); it != tabsInfo_.end(); ++it)
            it.key()->autoIndentation = true;
    }
    else
    {
        QHash<TextEdit*,tabInfo*>::iterator it;
        for (it = tabsInfo_.begin(); it != tabsInfo_.end(); ++it)
            it.key()->autoIndentation = false;
    }
}
/*************************/
void FPwin::encodingToCheck (const QString& encoding)
{
    if (encoding != "UTF-8")
        ui->actionOther->setDisabled (true);

    if (encoding == "UTF-8")
        ui->actionUTF_8->setChecked (true);
    else if (encoding == "UTF-16")
        ui->actionUTF_16->setChecked (true);
    else if (encoding == "CP1256")
        ui->actionWindows_Arabic->setChecked (true);
    else if (encoding == "ISO-8859-1")
        ui->actionISO_8859_1->setChecked (true);
    else if (encoding == "ISO-8859-15")
        ui->actionISO_8859_15->setChecked (true);
    else if (encoding == "CP1252")
        ui->actionWindows_1252->setChecked (true);
    else if (encoding == "CP1251")
        ui->actionCryllic_CP1251->setChecked (true);
    else if (encoding == "KOI8-U")
        ui->actionCryllic_KOI8_U->setChecked (true);
    else if (encoding == "ISO-8859-5")
        ui->actionCryllic_ISO_8859_5->setChecked (true);
    else if (encoding == "BIG5")
        ui->actionChineese_BIG5->setChecked (true);
    else if (encoding == "B18030")
        ui->actionChinese_GB18030->setChecked (true);
    else if (encoding == "ISO-2022-JP")
        ui->actionJapanese_ISO_2022_JP->setChecked (true);
    else if (encoding == "ISO-2022-JP-2")
        ui->actionJapanese_ISO_2022_JP_2->setChecked (true);
    else if (encoding == "ISO-2022-KR")
        ui->actionJapanese_ISO_2022_KR->setChecked (true);
    else if (encoding == "CP932")
        ui->actionJapanese_CP932->setChecked (true);
    else if (encoding == "EUC-JP")
        ui->actionJapanese_EUC_JP->setChecked (true);
    else if (encoding == "CP949")
        ui->actionKorean_CP949->setChecked (true);
    else if (encoding == "CP1361")
        ui->actionKorean_CP1361->setChecked (true);
    else if (encoding == "EUC-KR")
        ui->actionKorean_EUC_KR->setChecked (true);
    else
    {
        ui->actionOther->setDisabled (false);
        ui->actionOther->setChecked (true);
    }
}
/*************************/
const QString FPwin::checkToEncoding() const
{
    QString encoding;

    if (ui->actionUTF_8->isChecked())
        encoding = "UTF-8";
    else if (ui->actionUTF_16->isChecked())
        encoding = "UTF-16";
    else if (ui->actionWindows_Arabic->isChecked())
        encoding = "CP1256";
    else if (ui->actionISO_8859_1->isChecked())
        encoding = "ISO-8859-1";
    else if (ui->actionISO_8859_15->isChecked())
        encoding = "ISO-8859-15";
    else if (ui->actionWindows_1252->isChecked())
        encoding = "CP1252";
    else if (ui->actionCryllic_CP1251->isChecked())
        encoding = "CP1251";
    else if (ui->actionCryllic_KOI8_U->isChecked())
        encoding = "KOI8-U";
    else if (ui->actionCryllic_ISO_8859_5->isChecked())
        encoding = "ISO-8859-5";
    else if (ui->actionChineese_BIG5->isChecked())
        encoding = "BIG5";
    else if (ui->actionChinese_GB18030->isChecked())
        encoding = "B18030";
    else if (ui->actionJapanese_ISO_2022_JP->isChecked())
        encoding = "ISO-2022-JP";
    else if (ui->actionJapanese_ISO_2022_JP_2->isChecked())
        encoding = "ISO-2022-JP-2";
    else if (ui->actionJapanese_ISO_2022_KR->isChecked())
        encoding = "ISO-2022-KR";
    else if (ui->actionJapanese_CP932->isChecked())
        encoding = "CP932";
    else if (ui->actionJapanese_EUC_JP->isChecked())
        encoding = "EUC-JP";
    else if (ui->actionKorean_CP949->isChecked())
        encoding == "CP949";
    else if (ui->actionKorean_CP1361->isChecked())
        encoding = "CP1361";
    else if (ui->actionKorean_EUC_KR->isChecked())
        encoding = "EUC-KR";
    else
        encoding = "UTF-8";

    return encoding;
}
/*************************/
void FPwin::docProp()
{
    QHash<TextEdit*,tabInfo*>::iterator it;
    if (ui->statusBar->isVisible())
    {
        for (it = tabsInfo_.begin(); it != tabsInfo_.end(); ++it)
        {
            disconnect (it.key(), &QPlainTextEdit::blockCountChanged, this, &FPwin::statusMsgWithLineCount);
            disconnect (it.key(), &QPlainTextEdit::selectionChanged, this, &FPwin::statusMsg);
        }
        ui->statusBar->setVisible (false);
        return;
    }

    int index = ui->tabWidget->currentIndex();
    if (index == -1) return;

    statusMsgWithLineCount (qobject_cast< TextEdit *>(ui->tabWidget->widget (index))
               ->document()->blockCount());
    for (it = tabsInfo_.begin(); it != tabsInfo_.end(); ++it)
    {
        connect (it.key(), &QPlainTextEdit::blockCountChanged, this, &FPwin::statusMsgWithLineCount);
        connect (it.key(), &QPlainTextEdit::selectionChanged, this, &FPwin::statusMsg);
    }

    ui->statusBar->setVisible (true);
    if (QToolButton *wordButton = ui->statusBar->findChild<QToolButton *>())
        wordButton->setVisible (true);
    wordButtonStatus();
}
/*************************/
// Set the status bar text according to the block count.
void FPwin::statusMsgWithLineCount (const int lines)
{
    int index = ui->tabWidget->currentIndex();
    TextEdit *textEdit = qobject_cast< TextEdit *>(ui->tabWidget->widget (index));
    tabInfo *tabinfo = tabsInfo_[textEdit];
    QLabel *statusLabel = ui->statusBar->findChild<QLabel *>();

    if (!tabinfo->prog.isEmpty())
        statusLabel->setText (tr ("<b>Encoding:</b> <i>%1</i>"
                                  "&nbsp;&nbsp;&nbsp;&nbsp;<b>Syntax:</b> <i>%2</i>"
                                  "&nbsp;&nbsp;&nbsp;&nbsp;<b>Lines:</b> <i>%3</i>"
                                  "&nbsp;&nbsp;&nbsp;&nbsp;<b>Sel. Chars.:</b> <i>%4</i>"
                                  "&nbsp;&nbsp;&nbsp;&nbsp;<b>Words:</b>")
                              .arg (tabinfo->encoding)
                              .arg (tabinfo->prog)
                              .arg (QString::number (lines))
                              .arg (textEdit->textCursor().selectedText().size()));
    else
        statusLabel->setText (tr ("<b>Encoding:</b> <i>%1</i>"
                                  "&nbsp;&nbsp;&nbsp;&nbsp;<b>Lines:</b> <i>%2</i>"
                                  "&nbsp;&nbsp;&nbsp;&nbsp;<b>Sel. Chars.:</b> <i>%3</i>"
                                  "&nbsp;&nbsp;&nbsp;&nbsp;<b>Words:</b>")
                              .arg (tabinfo->encoding)
                              .arg (QString::number (lines))
                              .arg (textEdit->textCursor().selectedText().size()));
}
/*************************/
// Change the status bar text when the selection changes.
void FPwin::statusMsg()
{
    QLabel *statusLabel = ui->statusBar->findChild<QLabel *>();
    QString charN;
    charN.setNum (qobject_cast< TextEdit *>(ui->tabWidget->currentWidget())
                  ->textCursor().selectedText().size());
    QString str = statusLabel->text();
    int i = str.indexOf ("Sel.");
    int j = str.indexOf ("</i>&nbsp;&nbsp;&nbsp;&nbsp;<b>Words");
    str.replace (i + 20, j - i - 20, charN);
    statusLabel->setText (str);
}
/*************************/
void FPwin::wordButtonStatus()
{
    QToolButton *wordButton = ui->statusBar->findChild<QToolButton *>();
    if (!wordButton) return;
    int index = ui->tabWidget->currentIndex();
    TextEdit *textEdit = qobject_cast< TextEdit *>(ui->tabWidget->widget (index));
    tabInfo *tabinfo = tabsInfo_[textEdit];
    if (wordButton->isVisible())
    {
        QLabel *statusLabel = ui->statusBar->findChild<QLabel *>();
        int words = tabinfo->wordNumber;
        if (words == -1)
        {
            words = textEdit->toPlainText()
                    .split (QRegExp("(\\s|\\n|\\r)+"), QString::SkipEmptyParts)
                    .count();
            tabinfo->wordNumber = words;
        }

        wordButton->setVisible (false);
        statusLabel->setText (tr ("%1 <i>%2</i>")
                              .arg (statusLabel->text())
                              .arg (words));
        connect (textEdit, &QPlainTextEdit::textChanged, this, &FPwin::wordButtonStatus);
    }
    else
    {
        disconnect (textEdit, &QPlainTextEdit::textChanged, this, &FPwin::wordButtonStatus);
        tabinfo->wordNumber = -1;
        wordButton->setVisible (true);
        statusMsgWithLineCount (textEdit->document()->blockCount());
    }
}
/*************************/
void FPwin::filePrint()
{
    int index = ui->tabWidget->currentIndex();
    if (index == -1) return;

    TextEdit *textEdit = qobject_cast< TextEdit *>(ui->tabWidget->widget (index));
    QPrinter printer (QPrinter::HighResolution);

    /* choose an appropriate name and directory */
    QString fileName = tabsInfo_[textEdit]->fileName;
    if (fileName.isEmpty())
    {
        QDir dir = QDir::home();
        fileName= dir.filePath ("Untitled");
    }
    if (printer.outputFormat() == QPrinter::PdfFormat)
        printer.setOutputFileName (fileName.append (".pdf"));
    /*else if (printer.outputFormat() == QPrinter::PostScriptFormat)
        printer.setOutputFileName (fileName.append (".ps"));*/

    QPrintDialog dlg (&printer, this);
    dlg.setWindowModality (Qt::WindowModal);
    if (textEdit->textCursor().hasSelection())
        dlg.setOption (QAbstractPrintDialog::PrintSelection);
    dlg.setWindowTitle (tr ("Print Document"));
    if (dlg.exec() == QDialog::Accepted)
        textEdit->print (&printer);
}
/*************************/
void FPwin::detachAndDropTab (QPoint& dropPos)
{
    int index = ui->tabWidget->currentIndex();
    if (index == -1) return;

    /* do nothing if the tab isn't dropped outside this window */
    QPoint winPos = pos();
    if (dropPos.x() >= winPos.x()
        && dropPos.y() >= winPos.y()
        && dropPos.x() <= winPos.x() + width()
        && dropPos.y() <= winPos.y() + height())
    {
        return;
    }

    /*****************************************************************
     ***** See if another FeatherPad window is on this desktop.  *****
     ***** If there is one, check if the tab is dropped into it. *****
     *****************************************************************/

    /* get all other FeatherPad windows on this desktop */
    long d = onWhichDesktop (winId());
    QList<int> onThisDesktop;
    FPsingleton *singleton = static_cast<FPsingleton*>(qApp);
    int i;
    for (i = 0; i < singleton->Wins.count(); ++i)
    {
        if (singleton->Wins.at (i) != this
            && onWhichDesktop (singleton->Wins.at (i)->winId()) == d)
        {
            onThisDesktop.append (i);
        }
    }

    /* remove from the list those windows
       that don't include dropPos...  */
    FPwin *anotherFP = NULL;
    int top = -1;
    int insertIndex = 0;
    i = 0;
    while (i < onThisDesktop.count())
    {
        winPos = singleton->Wins.at (onThisDesktop.at (i))->pos();
        if (dropPos.x() < winPos.x()
            || dropPos.y() < winPos.y()
            || dropPos.x() > winPos.x() + singleton->Wins.at (onThisDesktop.at (i))->width()
            || dropPos.y() > winPos.y() + singleton->Wins.at (onThisDesktop.at (i))->height())
        {
            onThisDesktop.removeAt (i);
        }
        else ++i;
    }
#if defined Q_WS_X11 || defined Q_OS_LINUX
    if (!onThisDesktop.isEmpty())
    {
        top = 0;
        int result = onThisDesktop.at (0);
        if (onThisDesktop.count() > 1)
        {
            /* ... and find the topmost one among the remaining windows */
            QList<Window> windows = listXWins (QX11Info::appRootWindow());
            int j;
            for (i = 0; i < onThisDesktop.count(); i++)
            {
                j = windows.indexOf (singleton->Wins.at (onThisDesktop.at (i))->winId());
                if (j > top)
                {
                    top = j;
                    result = onThisDesktop.at (i);
                }
            }
        }

        anotherFP = singleton->Wins.at (result);
        /* do nothing if the target has a modal dialog */
        if (!anotherFP->findChildren<QFileDialog*>().isEmpty()
            || !anotherFP->findChildren<QDialog*>().isEmpty()
            || !anotherFP->findChildren<QMessageBox*>().isEmpty())
        {
            return;
        }
        /*int curIndex = anotherFP->ui->tabWidget->currentIndex();
        if (curIndex > -1)
        {
            TextEdit *anotherTE = qobject_cast< TextEdit *>(anotherFP->ui->tabWidget->widget (curIndex));
            if (anotherTE->document()->isEmpty() && !anotherTE->document()->isModified())
                anotherFP->closeTabAtIndex (curIndex);
        }*/
        insertIndex = anotherFP->ui->tabWidget->currentIndex() + 1;
    }
#endif

    if (top == -1 && ui->tabWidget->count() == 1) return;

    /*****************************************************
     *****          Get all necessary info.          *****
     ***** Then, remove the tab but keep its widget. *****
     *****************************************************/

    QString tooltip = ui->tabWidget->tabToolTip (index);
    QString tabText = ui->tabWidget->tabText (index);
    QString title = windowTitle();
    bool hl = true;
    bool spin = false;
    bool ln = false;
    bool status = false;
    if (!ui->actionSyntax->isChecked())
        hl = false;
    if (ui->spinBox->isVisible())
        spin = true;
    if (ui->actionLineNumbers->isChecked())
        ln = true;
    if (ui->statusBar->isVisible())
        status = true;

    TextEdit *textEdit = qobject_cast< TextEdit *>(ui->tabWidget->widget (index));

    /* disconnect all signals except for syntax highlighting */
    disconnect (textEdit, &QPlainTextEdit::updateRequest, this ,&FPwin::hlighting);
    disconnect (textEdit, &QPlainTextEdit::textChanged, this ,&FPwin::hlight);
    disconnect (textEdit, &QPlainTextEdit::textChanged, this, &FPwin::wordButtonStatus);
    disconnect (textEdit, &QPlainTextEdit::blockCountChanged, this, &FPwin::statusMsgWithLineCount);
    disconnect (textEdit, &QPlainTextEdit::selectionChanged, this, &FPwin::statusMsg);
    disconnect (textEdit, &QPlainTextEdit::copyAvailable, ui->actionCut, &QAction::setEnabled);
    disconnect (textEdit, &QPlainTextEdit::copyAvailable, ui->actionDelete, &QAction::setEnabled);
    disconnect (textEdit, &QPlainTextEdit::copyAvailable, ui->actionCopy, &QAction::setEnabled);
    disconnect (textEdit, &TextEdit::fileDropped, this, &FPwin::newTabFromName);
    disconnect (textEdit, &QPlainTextEdit::cursorPositionChanged, this, &FPwin::matchBrackets);

    disconnect (textEdit->document(), &QTextDocument::blockCountChanged, this, &FPwin::setMax);
    disconnect (textEdit->document(), &QTextDocument::modificationChanged, this, &FPwin::asterisk);
    disconnect (textEdit->document(), &QTextDocument::undoAvailable, ui->actionUndo, &QAction::setEnabled);
    disconnect (textEdit->document(), &QTextDocument::redoAvailable, ui->actionRedo, &QAction::setEnabled);
    disconnect (textEdit->document(), &QTextDocument::modificationChanged, ui->actionSave, &QAction::setEnabled);

    tabInfo *tabinfo = tabsInfo_[textEdit];
    tabsInfo_.remove (textEdit);
    ui->tabWidget->removeTab (index);
    int count = ui->tabWidget->count();
    if (count == 1)
        ui->actionDetach->setDisabled (true);

    /******************************************************************************
     ***** If the tab is dropped into another window, insert it as a new tab. *****
     ***** Otherwise, create a new window and replace its tab by this widget. *****
     ******************************************************************************/

    if (!anotherFP)
    {
        anotherFP = singleton->newWin ("");
        anotherFP->closeTabAtIndex (0);
    }

    /* first, set the new info... */
    anotherFP->lastFile_ = tabinfo->fileName;
    tabinfo->greenSel = QList<QTextEdit::ExtraSelection>();
    anotherFP->tabsInfo_[textEdit] = tabinfo;
    /* ... then insert the detached widget... */
    anotherFP->ui->tabWidget->insertTab (insertIndex, textEdit, tabText);
    /* ... and remove all yellow and green highlights
       (the yellow ones will be recreated later if needed) */
    QList<QTextEdit::ExtraSelection> extraSelections;
    if ((top == -1 && (ln || spin))
        || (top > -1 && (ln || spin)
            && (anotherFP->ui->actionLineNumbers->isChecked() || anotherFP->ui->spinBox->isVisible())))
    {
        extraSelections.prepend (textEdit->currentLine);
    }
    extraSelections.append (tabinfo->redSel);
    textEdit->setExtraSelections (extraSelections);

    /* at last, set all properties correctly */
    if (anotherFP->ui->tabWidget->count() == 1)
        anotherFP->enableWidgets (true);
    if (top == -1)
        anotherFP->setWindowTitle (title);
    anotherFP->ui->tabWidget->setTabToolTip (insertIndex, tooltip);
    /* detach button */
    if (top > -1 && anotherFP->ui->tabWidget->count() == 2)
        anotherFP->ui->actionDetach->setEnabled (true);
    /* reload buttons, syntax highlighting, jump bar, line numbers */
    if (top == -1)
    {
        anotherFP->encodingToCheck (tabinfo->encoding);
        if (!tabinfo->fileName.isEmpty())
            anotherFP->ui->actionReload->setEnabled (true);
        if (!hl)
            anotherFP->ui->actionSyntax->setChecked (false);
        if (spin)
        {
            anotherFP->ui->spinBox->setVisible (true);
            anotherFP->ui->label->setVisible (true);
            anotherFP->ui->spinBox->setMaximum (textEdit->document()->blockCount());
            connect (textEdit->document(), &QTextDocument::blockCountChanged, anotherFP, &FPwin::setMax);
        }
        if (ln)
            anotherFP->ui->actionLineNumbers->setChecked (true);
    }
    else
    {
        if (anotherFP->ui->actionSyntax->isChecked() && !tabinfo->highlighter)
            anotherFP->syntaxHighlighting (insertIndex);
        else if (!anotherFP->ui->actionSyntax->isChecked() && tabinfo->highlighter)
        {
            Highlighter *highlighter = tabinfo->highlighter;
            tabinfo->highlighter = NULL;
            delete highlighter; highlighter = NULL;
        }
        if (anotherFP->ui->spinBox->isVisible())
            connect (textEdit->document(), &QTextDocument::blockCountChanged, anotherFP, &FPwin::setMax);
        if (anotherFP->ui->actionLineNumbers->isChecked() || anotherFP->ui->spinBox->isVisible())
            textEdit->showLineNumbers (true);
        else
            textEdit->showLineNumbers (false);
    }
    /* searching */
    if (!tabinfo->searchEntry.isEmpty())
    {
        connect (textEdit, &QPlainTextEdit::textChanged, anotherFP, &FPwin::hlight);
        connect (textEdit, &QPlainTextEdit::updateRequest, anotherFP, &FPwin::hlighting);
        /* restore yellow highlights, which will automatically
           set the current line highlight if needed because the
           spin button and line number menuitem are set above */
        anotherFP->hlight();
    }
    /* status bar */
    if (top == -1)
    {
        if (status)
        {
            anotherFP->ui->statusBar->setVisible (true);
            anotherFP->statusMsgWithLineCount (textEdit->document()->blockCount());
            if (tabinfo->wordNumber == -1)
            {
                if (QToolButton *wordButton = anotherFP->ui->statusBar->findChild<QToolButton *>())
                    wordButton->setVisible (true);
            }
            else
            {
                if (QToolButton *wordButton = anotherFP->ui->statusBar->findChild<QToolButton *>())
                    wordButton->setVisible (false);
                QLabel *statusLabel = anotherFP->ui->statusBar->findChild<QLabel *>();
                statusLabel->setText (tr ("%1 <i>%2</i>")
                                      .arg (statusLabel->text())
                                      .arg (tabinfo->wordNumber));
                connect (textEdit, &QPlainTextEdit::textChanged, anotherFP, &FPwin::wordButtonStatus);
            }
            connect (textEdit, &QPlainTextEdit::blockCountChanged, anotherFP, &FPwin::statusMsgWithLineCount);
            connect (textEdit, &QPlainTextEdit::selectionChanged, anotherFP, &FPwin::statusMsg);
        }
        if (textEdit->lineWrapMode() == QPlainTextEdit::NoWrap)
            anotherFP->ui->actionWrap->setChecked (false);
    }
    else
    {
        if (anotherFP->ui->statusBar->isVisible())
        {
            connect (textEdit, &QPlainTextEdit::blockCountChanged, anotherFP, &FPwin::statusMsgWithLineCount);
            connect (textEdit, &QPlainTextEdit::selectionChanged, anotherFP, &FPwin::statusMsg);
            if (tabinfo->wordNumber != 1)
                connect (textEdit, &QPlainTextEdit::textChanged, anotherFP, &FPwin::wordButtonStatus);
        }
        if (anotherFP->ui->actionWrap->isChecked() && textEdit->lineWrapMode() == QPlainTextEdit::NoWrap)
            textEdit->setLineWrapMode (QPlainTextEdit::WidgetWidth);
        else if (!anotherFP->ui->actionWrap->isChecked() && textEdit->lineWrapMode() == QPlainTextEdit::WidgetWidth)
            textEdit->setLineWrapMode (QPlainTextEdit::NoWrap);
    }
    /* auto indentation */
    if (top == -1)
    {
        if (textEdit->autoIndentation == false)
            anotherFP->ui->actionIndent->setChecked (false);
    }
    else
    {
        if (anotherFP->ui->actionIndent->isChecked() && textEdit->autoIndentation == false)
            textEdit->autoIndentation = true;
        else if (!anotherFP->ui->actionIndent->isChecked() && textEdit->autoIndentation == true)
            textEdit->autoIndentation = false;
    }
    /* the remaining signals */
    connect (textEdit->document(), &QTextDocument::undoAvailable, anotherFP->ui->actionUndo, &QAction::setEnabled);
    connect (textEdit->document(), &QTextDocument::redoAvailable, anotherFP->ui->actionRedo, &QAction::setEnabled);
    connect (textEdit->document(), &QTextDocument::modificationChanged, anotherFP->ui->actionSave, &QAction::setEnabled);
    connect (textEdit->document(), &QTextDocument::modificationChanged, anotherFP, &FPwin::asterisk);
    connect (textEdit, &QPlainTextEdit::copyAvailable, anotherFP->ui->actionCopy, &QAction::setEnabled);
    if (!textEdit->isReadOnly())
    {
        connect (textEdit, &QPlainTextEdit::copyAvailable, anotherFP->ui->actionCut, &QAction::setEnabled);
        connect (textEdit, &QPlainTextEdit::copyAvailable, anotherFP->ui->actionDelete, &QAction::setEnabled);
    }
    connect (textEdit, &TextEdit::fileDropped, anotherFP, &FPwin::newTabFromName);
    connect (textEdit, &QPlainTextEdit::cursorPositionChanged, anotherFP, &FPwin::matchBrackets);

    if (top > -1)
        anotherFP->ui->tabWidget->setCurrentIndex (insertIndex);

    textEdit->setFocus();

    anotherFP->activateWindow();
    anotherFP->raise();

    if (count == 0) close();
}
/*************************/
void FPwin::tabContextMenu (const QPoint& p)
{
    int tabNum = ui->tabWidget->count();
    if (tabNum == 1) return;

    QTabBar *tbar = ui->tabWidget->tabBar();
    rightClicked_ = tbar->tabAt (p);
    if (rightClicked_ < 0) return;

    QMenu menu;
    if (rightClicked_ < tabNum - 1)
        menu.addAction (ui->actionCloseRight);
    if (rightClicked_ > 0)
        menu.addAction (ui->actionCloseLeft);
    menu.addSeparator();
    if (rightClicked_ < tabNum - 1 && rightClicked_ > 0)
        menu.addAction (ui->actionCloseOther);
    menu.addAction (ui->actionCloseAll);
    menu.exec (tbar->mapToGlobal (p));
}
/*************************/
void FPwin::detachTab()
{
    QRect screenRect = QApplication::desktop()->screenGeometry();
    QPoint point = QPoint (2*screenRect.width(), 2*screenRect.height());
    detachAndDropTab (point);
}
/*************************/
void FPwin::prefDialog()
{
    PrefDialog *dlg = new PrefDialog (this);
    /*dlg->show();
    move (x() + width()/2 - dlg->width()/2,
          y() + height()/2 - dlg->height()/ 2);*/
    dlg->exec();
}
/*************************/
void FPwin::aboutDialog()
{
    QMessageBox::about (this, "About FeatherPad",
                        tr ("<center><b><big>FeatherPad 0.5.6</big></b></center><br>"\
                            "<center> A lightweight, tabbed, plain-text editor </center>\n"\
                            "<center> based on Qt5 </center><br>"\
                            "<center> Author: <a href='mailto:tsujan2000@gmail.com?Subject=My%20Subject'>Pedram Pourang (aka. Tsu Jan)</a> </center><p></p>"));
}
/*************************/
void FPwin::helpDoc()
{
    int index = ui->tabWidget->currentIndex();
    if (index == -1)
        newTab();
    else
    {
        QHash<TextEdit*,tabInfo*>::iterator it;
        for (it = tabsInfo_.begin(); it != tabsInfo_.end(); ++it)
        {
            if (it.value()->fileName.isEmpty()
                && !it.key()->document()->isModified()
                && !it.key()->document()->isEmpty())
            {
                ui->tabWidget->setCurrentIndex (ui->tabWidget->indexOf (it.key()));
                return;
            }
        }
    }

    QFile helpFile (DATADIR "/featherpad/help");

    if (!helpFile.exists()) return;
    if (!helpFile.open (QFile::ReadOnly)) return;

    TextEdit *textEdit = qobject_cast< TextEdit *>(ui->tabWidget->currentWidget());
    if (!textEdit->document()->isEmpty()
        || textEdit->document()->isModified())
    {
        newTab();
        textEdit = qobject_cast< TextEdit *>(ui->tabWidget->currentWidget());
    }

    QByteArray data = helpFile.readAll();
    helpFile.close();
    QTextCodec *codec = QTextCodec::codecForName ("UTF-8");
    QString str = codec->toUnicode (data);
    textEdit->setPlainText (str);

    textEdit->setReadOnly (true);
    textEdit->viewport()->setStyleSheet (".QWidget {"
                                         "color: black;"
                                         "background-color: rgb(225, 238, 255);}");
    ui->actionCut->setDisabled (true);
    ui->actionPaste->setDisabled (true);
    ui->actionDelete->setDisabled (true);
    disconnect (textEdit, &QPlainTextEdit::copyAvailable, ui->actionCut, &QAction::setEnabled);
    disconnect (textEdit, &QPlainTextEdit::copyAvailable, ui->actionDelete, &QAction::setEnabled);

    index = ui->tabWidget->currentIndex();
    tabInfo *tabinfo = tabsInfo_[textEdit];
    tabinfo->encoding = "UTF-8";
    tabinfo->wordNumber = -1;
    if (ui->statusBar->isVisible())
    {
        statusMsgWithLineCount (textEdit->document()->blockCount());
        if (QToolButton *wordButton = ui->statusBar->findChild<QToolButton *>())
            wordButton->setVisible (true);
    }
    encodingToCheck ("UTF-8");
    ui->tabWidget->setTabText (index, tr ("%1").arg ("** Help **"));
    setWindowTitle (tr ("%1[*]").arg ("** Help **"));
    setWindowModified (false);
    ui->tabWidget->setTabToolTip (index, "** Help **");

    if (!tabinfo->searchEntry.isEmpty())
        hlight();
}

}
