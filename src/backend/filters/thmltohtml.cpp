/*********
*
* This file is part of BibleTime's source code, http://www.bibletime.info/.
*
* Copyright 1999-2014 by the BibleTime developers.
* The BibleTime source code is licensed under the GNU General Public License version 2.0.
*
**********/

#include "backend/filters/thmltohtml.h"

#include <QString>
#include <QRegExp>
#include <QUrl>
#include <QTextCodec>
#include "backend/config/btconfig.h"
#include "backend/drivers/cswordmoduleinfo.h"
#include "backend/managers/clanguagemgr.h"
#include "backend/managers/referencemanager.h"
#include "backend/managers/cswordbackend.h"

// Sword includes:
#include <swmodule.h>
#include <utilstr.h>
#include <utilxml.h>
#include <versekey.h>


namespace Filters {

ThmlToHtml::ThmlToHtml() {
    setEscapeStringCaseSensitive(true);
    setPassThruUnknownEscapeString(true); //the HTML widget will render the HTML escape codes

    setTokenStart("<");
    setTokenEnd(">");
    setTokenCaseSensitive(true);

    addTokenSubstitute("/foreign", "</span>");

    removeTokenSubstitute("note");
    removeTokenSubstitute("/note");
}

char ThmlToHtml::processText(sword::SWBuf &buf, const sword::SWKey *key,
                             const sword::SWModule *module)
{
    sword::ThMLHTML::processText(buf, key, module);

    CSwordModuleInfo* m = CSwordBackend::instance()->findModuleByName( module->getName() );

    if (m && !(m->has(CSwordModuleInfo::lemmas) || m->has(CSwordModuleInfo::strongNumbers))) { //only parse if the module has strongs or lemmas
        return 1;
    }

    QString result;

    QString t = QString::fromUtf8(buf.c_str());
    QRegExp tag("([.,;]?<sync[^>]+(type|value)=\"([^\"]+)\"[^>]+(type|value)=\"([^\"]+)\"([^<]*)>)+");

    QStringList list;
    int lastMatchEnd = 0;
    int pos = tag.indexIn(t, 0);

    if (pos == -1) { //no strong or morph code found in this text
        return 1; //WARNING: Return already here
    }

    while (pos != -1) {
        list.append(t.mid(lastMatchEnd, pos + tag.matchedLength() - lastMatchEnd));

        lastMatchEnd = pos + tag.matchedLength();
        pos = tag.indexIn(t, pos + tag.matchedLength());
    }

    if (!t.right(t.length() - lastMatchEnd).isEmpty()) {
        list.append(t.right(t.length() - lastMatchEnd));
    }

    tag = QRegExp("<sync[^>]+(type|value|class)=\"([^\"]+)\"[^>]+(type|value|class)=\"([^\"]+)\"[^>]+((type|value|class)=\"([^\"]+)\")*([^<]*)>");

    for (QStringList::iterator it = list.begin(); it != list.end(); ++it) {
        QString e( *it );

        const bool textPresent = (e.trimmed().remove(QRegExp("[.,;:]")).left(1) != "<");

        if (!textPresent) {
            continue;
        }


        bool hasLemmaAttr = false;
        bool hasMorphAttr = false;

        int pos = tag.indexIn(e, 0);
        bool insertedTag = false;
        QString value;
        QString valueClass;

        while (pos != -1) {
            bool isMorph = false;
            bool isStrongs = false;
            value = QString::null;
            valueClass = QString::null;

            // check 3 attribute/value pairs

            for (int i = 1; i < 6; i += 2) {
                if (i > 4)
                    i++;

                if (tag.cap(i) == "type") {
                    isMorph   = (tag.cap(i + 1) == "morph");
                    isStrongs = (tag.cap(i + 1) == "Strongs");
                }
                else if (tag.cap(i) == "value") {
                    value = tag.cap(i + 1);
                }
                else if (tag.cap(i) == "class") {
                    valueClass = tag.cap(i + 1);
                }
            }

            // prepend the class qualifier to the value
            if (!valueClass.isEmpty()) {
                value = valueClass + ":" + value;
                //     value.append(":").append(value);
            }

            if (value.isEmpty()) {
                break;
            }

            //insert the span
            if (!insertedTag) {
                e.replace(pos, tag.matchedLength(), "</span>");
                pos += 7;

                QString rep = QString("<span lemma=\"").append(value).append("\">");
                int startPos = 0;
                QChar c = e[startPos];

                while ((startPos < pos) && (c.isSpace() || c.isPunct())) {
                    ++startPos;
                    c = e[startPos];
                }

                hasLemmaAttr = isStrongs;
                hasMorphAttr = isMorph;

                e.insert( startPos, rep );
                pos += rep.length();
            }
            else { //add the attribute to the existing tag
                e.remove(pos, tag.matchedLength());

                if ((!isMorph && hasLemmaAttr) || (isMorph && hasMorphAttr)) { //we append another attribute value, e.g. 3000 gets 3000|5000
                    //search the existing attribute start
                    QRegExp attrRegExp( isMorph ? "morph=\".+(?=\")" : "lemma=\".+(?=\")" );
                    attrRegExp.setMinimal(true);
                    const int foundAttrPos = e.indexOf(attrRegExp, pos);

                    if (foundAttrPos != -1) {
                        e.insert(foundAttrPos + attrRegExp.matchedLength(), QString("|").append(value));
                        pos += value.length() + 1;

                        hasLemmaAttr = !isMorph;
                        hasMorphAttr = isMorph;
                    }
                }
                else { //attribute was not yet inserted
                    const int attrPos = e.indexOf(QRegExp("morph=|lemma="), 0);

                    if (attrPos >= 0) {
                        QString attr;
                        attr.append(isMorph ? "morph" : "lemma").append("=\"").append(value).append("\" ");
                        e.insert(attrPos, attr);

                        hasMorphAttr = isMorph;
                        hasLemmaAttr = !isMorph;

                        pos += attr.length();
                    }
                }
            }

            insertedTag = true;
            pos = tag.indexIn(e, pos);
        }

        result.append( e );
    }

    if (list.count()) {
        buf = (const char*)result.toUtf8();
    }

    return 1;
}


bool ThmlToHtml::handleToken(sword::SWBuf &buf, const char *token,
                             sword::BasicFilterUserData *userData)
{
    if (!substituteToken(buf, token) && !substituteEscapeString(buf, token)) {
        sword::XMLTag tag(token);
        UserData* myUserData = dynamic_cast<UserData*>(userData);
        sword::SWModule* myModule = const_cast<sword::SWModule*>(myUserData->module); //hack to be able to call stuff like Lang()

        if ( tag.getName() && !sword::stricmp(tag.getName(), "foreign") ) { // a text part in another language, we have to set the right font

            if (tag.getAttribute("lang")) {
                const char* abbrev = tag.getAttribute("lang");
                //const CLanguageMgr::Language* const language = CLanguageMgr::instance()->languageForAbbrev( QString::fromLatin1(abbrev) );

                buf.append("<span class=\"foreign\" lang=\"");
                buf.append(abbrev);
                buf.append("\">");
            }
        }
        else if (tag.getName() && !sword::stricmp(tag.getName(), "sync")) { //lemmas, morph codes or strongs

            if (tag.getAttribute("type") && (!sword::stricmp(tag.getAttribute("type"), "morph") || !sword::stricmp(tag.getAttribute("type"), "Strongs") || !sword::stricmp(tag.getAttribute("type"), "lemma"))) { // Morph or Strong
                buf.append('<');
                buf.append(token);
                buf.append('>');
            }
        }
        else if (tag.getName() && !sword::stricmp(tag.getName(), "note")) { // <note> tag

            if (!tag.isEndTag() && !tag.isEmpty()) {
                //appending is faster than appendFormatted
                buf.append(" <span class=\"footnote\" note=\"");
                buf.append(myModule->getName());
                buf.append('/');
                buf.append(myUserData->key->getShortText());
                buf.append('/');
                buf.append( QString::number(myUserData->swordFootnote++).toUtf8().constData() );
                buf.append("\">*</span> ");

                myUserData->suspendTextPassThru = true;
                myUserData->inFootnoteTag = true;
            }
            else if (tag.isEndTag() && !tag.isEmpty()) { //end tag
                //buf += ")</span>";
                myUserData->suspendTextPassThru = false;
                myUserData->inFootnoteTag = false;
            }
        }
        else if (tag.getName() && !sword::stricmp(tag.getName(), "scripRef")) { // a scripRef
            //scrip refs which are embeded in footnotes may not be displayed!

            if (!myUserData->inFootnoteTag) {
                if (tag.isEndTag()) {
                    if (myUserData->inscriptRef) { // like "<scripRef passage="John 3:16">See John 3:16</scripRef>"
                        buf.append("</a></span>");

                        myUserData->inscriptRef = false;
                        myUserData->suspendTextPassThru = false;
                    }
                    else { // like "<scripRef>John 3:16</scripRef>"

                        CSwordModuleInfo* mod = btConfig().getDefaultSwordModuleByType("standardBible");
                        //Q_ASSERT(mod); tested later
                        if (mod) {
                            ReferenceManager::ParseOptions options;
                            options.refBase = QString::fromUtf8(myUserData->key->getText()); //current module key
                            options.refDestinationModule = QString(mod->name());
                            options.sourceLanguage = QString(myModule->getLanguage());
                            options.destinationLanguage = QString("en");

                            //it's ok to split the reference, because to descriptive text is given
                            bool insertSemicolon = false;
                            buf.append("<span class=\"crossreference\">");
                            QStringList refs = QString::fromUtf8(myUserData->lastTextNode.c_str()).split(";");
                            QString oldRef; //the previous reference to use as a base for the next refs
                            for (QStringList::iterator it(refs.begin()); it != refs.end(); ++it) {

                                if (! oldRef.isEmpty() ) {
                                    options.refBase = oldRef; //use the last ref as a base, e.g. Rom 1,2-3, when the next ref is only 3:3-10
                                }
                                const QString completeRef( ReferenceManager::parseVerseReference((*it), options) );

                                oldRef = completeRef; //use the parsed result as the base for the next ref.

                                if (insertSemicolon) { //prepend a ref divider if we're after the first one
                                    buf.append("; ");
                                }

                                buf.append("<a href=\"");
                                buf.append(
                                    ReferenceManager::encodeHyperlink(
                                        mod->name(),
                                        completeRef,
                                        ReferenceManager::typeFromModule(mod->type())
                                    ).toUtf8().constData()
                                );

                                buf.append("\" crossrefs=\"");
                                buf.append((const char*)completeRef.toUtf8());
                                buf.append("\">");

                                buf.append((const char*)(*it).toUtf8());

                                buf.append("</a>");

                                insertSemicolon = true;
                            }
                            buf.append("</span>"); //crossref end
                        }

                        myUserData->suspendTextPassThru = false;
                    }
                }
                else if (tag.getAttribute("passage") ) { //the passage was given as a parameter value
                    myUserData->inscriptRef = true;
                    myUserData->suspendTextPassThru = false;

                    const char* ref = tag.getAttribute("passage");
                    Q_ASSERT(ref);

                    CSwordModuleInfo* mod = btConfig().getDefaultSwordModuleByType("standardBible");
                    //Q_ASSERT(mod); tested later

                    ReferenceManager::ParseOptions options;
                    options.refBase = QString::fromUtf8(myUserData->key->getText());

                    options.sourceLanguage = myModule->getLanguage();
                    options.destinationLanguage = QString("en");

                    const QString completeRef = ReferenceManager::parseVerseReference(QString::fromUtf8(ref), options);

                    if (mod) {
                        options.refDestinationModule = QString(mod->name());
                        buf.append("<span class=\"crossreference\">");
                        buf.append("<a href=\"");
                        buf.append(
                            ReferenceManager::encodeHyperlink(
                                mod->name(),
                                completeRef,
                                ReferenceManager::typeFromModule(mod->type())
                            ).toUtf8().constData()
                        );
                        buf.append("\" crossrefs=\"");
                        buf.append((const char*)completeRef.toUtf8());
                        buf.append("\">");
                    }
                    else {
                        buf.append("<span><a>");
                    }
                }
                else if ( !tag.getAttribute("passage") ) { // we're starting a scripRef like "<scripRef>John 3:16</scripRef>"
                    myUserData->inscriptRef = false;

                    // let's stop text from going to output, the text get's added in the -tag handler
                    myUserData->suspendTextPassThru = true;
                }
            }
        }
        else if (tag.getName() && !sword::stricmp(tag.getName(), "div")) {
            if (tag.isEndTag()) {
                buf.append("</div>");
            }
            else if ( tag.getAttribute("class") && !sword::stricmp(tag.getAttribute("class"), "sechead") ) {
                buf.append("<div class=\"sectiontitle\">");
            }
            else if (tag.getAttribute("class") && !sword::stricmp(tag.getAttribute("class"), "title")) {
                buf.append("<div class=\"booktitle\">");
            }
        }
        else if (tag.getName() && !sword::stricmp(tag.getName(), "img") && tag.getAttribute("src")) {
            const char* value = tag.getAttribute("src");

            if (value[0] == '/') {
                value++; //strip the first /
            }

            buf.append("<img src=\"");
            QString absPath(QTextCodec::codecForLocale()->toUnicode(myUserData->module->getConfigEntry("AbsoluteDataPath")));
            QString relPath(QString::fromUtf8(value));
            QString url(QUrl::fromLocalFile(absPath.append('/').append(relPath)).toString());
            buf.append(url.toUtf8().data());
            buf.append("\" />");
        }
        else { // let unknown token pass thru
            return sword::ThMLHTML::handleToken(buf, token, userData);
        }
    }

    return true;
}

} // namespace Filtes
