/*********
*
* In the name of the Father, and of the Son, and of the Holy Spirit.
*
* This file is part of BibleTime's source code, http://www.bibletime.info/.
*
* Copyright 1999-2014 by the BibleTime developers.
* The BibleTime source code is licensed under the GNU General Public License
* version 2.0.
*
**********/

#include "backend/bookshelfmodel/languageitem.h"

#include "util/geticon.h"


namespace BookshelfModel {

QVariant LanguageItem::data(int role) const {
    switch (role) {

        case Qt::DisplayRole:
            return m_language->translatedName();

        case Qt::DecorationRole:
            return util::getIcon("flag.svg");

        default:
            return Item::data(role);

    }
}

} // namespace BookshelfModel
