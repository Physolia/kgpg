/***************************************************************************
                          keygen.h  -  description
                             -------------------
    begin                : Mon Jul 8 2002
    copyright            : (C) 2002 by y0k0
    email                : bj@altern.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#ifndef KEYGEN_H
#define KEYGEN_H


#include <kdialogbase.h>
#include <klocale.h>

class QCheckBox;

class KComboBox;
class KLineEdit;


class keyGenerate : public KDialogBase
{
        Q_OBJECT
public:
        keyGenerate(QWidget *parent=0, const char *name=0);
        QCheckBox *choix3,*choix4,*choix2,*choix1,*choix5;

        KComboBox *keykind,*keysize,*keyexp;
        KLineEdit *numb,*comment,*kname,*mail;
        bool expert;

public slots:
        void slotOk();
        void slotApply();
        void activateexp(int state);
        bool getmode();
        QString getkeycomm();
        QString getkeynumb();
        QString getkeymail();
        QString getkeyname();
        QString getkeysize();
        QString getkeytype();
        int getkeyexp();
};

#endif
