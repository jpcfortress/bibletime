/*********
*
* This file is part of BibleTime's source code, http://www.bibletime.info/.
*
* Copyright 1999-2010 by the BibleTime developers.
* The BibleTime source code is licensed under the GNU General Public License version 2.0.
*
**********/

#ifndef BTBIBLEKEYWIDGET_H
#define BTBIBLEKEYWIDGET_H

#include <QWidget>

#include <QTimer>
#include "backend/drivers/cswordbiblemoduleinfo.h"
#include "frontend/keychooser/cscrollerwidgetset.h"


class BtDropdownChooserButton;
class CLexiconKeyChooser;
class CSwordVerseKey;
class QLineEdit;

class BtBibleKeyWidget : public QWidget  {
        Q_OBJECT

    public:
        BtBibleKeyWidget(const CSwordBibleModuleInfo *module,
                         CSwordVerseKey *key, QWidget *parent = 0,
                         const char *name = 0);

        ~BtBibleKeyWidget();
        bool setKey(CSwordVerseKey* key);
        void setModule(const CSwordBibleModuleInfo *m = 0);
        bool eventFilter(QObject *o, QEvent *e);

    signals:
        void beforeChange(CSwordVerseKey* key);
        void changed(CSwordVerseKey* key);

    protected:
        void enterEvent(QEvent *event);
        void leaveEvent(QEvent *event);
        void resizeEvent(QResizeEvent *event);
        void resetDropDownButtons();

    protected slots: // Protected slots
        /**
        * Is called when the return key was presed in the textbox.
        */
        void slotReturnPressed();

        void slotClearRef();

        void slotUpdateLock();
        void slotUpdateUnlock();
        void slotStepBook(int);
        void slotStepChapter(int);
        void slotStepVerse(int);
        void slotChangeBook(QString bookname);
        void slotChangeChapter(int chapter);
        void slotChangeVerse(int verse);

    public slots:
        void updateText();

    private:
        friend class CLexiconKeyChooser;
        friend class BtDropdownChooserButton;
        friend class BtBookDropdownChooserButton;
        friend class BtChapterDropdownChooserButton;
        friend class BtVerseDropdownChooserButton;

        CSwordVerseKey *m_key;

        QLineEdit* m_textbox;

        CScrollerWidgetSet *m_bookScroller;
        CScrollerWidgetSet *m_chapterScroller;
        CScrollerWidgetSet *m_verseScroller;

        QWidget *m_dropDownButtons;
        QTimer m_dropDownHoverTimer;
        BtDropdownChooserButton* m_bookDropdownButton;
        BtDropdownChooserButton* m_chapterDropdownButton;
        BtDropdownChooserButton* m_verseDropdownButton;

        bool updatelock;
        QString oldKey;
        const CSwordBibleModuleInfo *m_module;
};

#endif