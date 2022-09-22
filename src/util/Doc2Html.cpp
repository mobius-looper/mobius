/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * A utility to convert an XML file written in a simple XML language
 * into an HTML file.  This is used for all of my product documentation
 * because I can't stand word processors.  It is rather old and I've
 * made many improvements to the Java verion that need to be ported someday.
 * 
 * There is no DTD right now, but it is pretty simple:
 *
 *   <document>
 *     <heading>
 *      <title>Document of Destiny</title>
 *      <author>Phineus Q. Public</author>
 *      <date>January 1, 1970</date>
 *     </heading>
 * 
 *   <!-- Including a TOC element here will cause the auto-generation of
 *        a hierarchical table of contents
 *   -->
 *   <TOC/>
 * 
 *   <section><title>Section 1</title>
 *   <p>This is a paragraph.  When entering text, formatting is similar
 *   to HTML.  You can use the "b" element for <b>boldface</b> words and
 *   the "i" element for <i>italicized</i> words.  You can use the "ref"
 *   element to create a link to another section, you can jump to 
 *   the second section here <ref>Section 2</ref>.
 *   </p>
 *   <pre>
 *      The "pre" element can be used to insert formatted text such
 *      as fragments of code.  
 *
 *          public static void main();
 *
 *      The will displayed as-is with no further formatting.  If you
 *      need to include XML markup in a "pre" element, you must remember
 *      to escape less than and ampersand characters.
 *
 *      &lt;Form>
 *        &lt;Field name='Bangers &amp; Mash'/>
 *      &lt;/Form>
 *
 *   </pre>
 *   <example title='Example 1: something with XML'>
 *      The "example" element is like "pre" but it allows a title and
 *      renders in a much more pleasing way using the W3C stylesheet.
 *   </example>
 *   <section><title>Section 2</title>
 *     <section><title>Sub-Section 1</title>
 *        <p>Sections may be nested to any level.  Section titles will
 *           be prefixed with an auto-generated number, for example
 *           1.1, 1.2, 1.2.1, 1.2.1.1, etc.
 *        </p>
 *     </section>
 *   </section>
 *   <section><title>Section 3</title>
 *
 *     <p>Bulleted lists may be entered this way:</p>
 *
 *     <ul>
 *       <li>bullet 1</li>
 *       <li>bullet 2</li>
 *     </ul>
 *
 *     <p>Numbered lists may be entered this way:</p>
 *
 *     <ol>
 *       <li>step 1</li>
 *       <li>step 2</li>
 *     </ol>
 *
 *     <p>A glossary of terms and definitions may be entered this way:</p>
 *
 *     <glossary>
 *       <gi><term>form</term>
 *         <def>An object encapsulating rules for the transformation
 *           of data between an external and an internal model.
 *         </def>
 *       </gi>
 *       <gi><term>rule</term>
 *         <def>An statement written in XPRESS or Javascript</def>
 *       </gi>
 *     </glossary>
 *   </section>
 *   </document>
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Util.h"
#include "List.h"
#include "Trace.h"
#include "XomParser.h"

/****************************************************************************
 *                                                                          *
 *   							   PREAMBLE                                 *
 *                                                                          *
 ****************************************************************************/

const char* HEAD_START = 
"<html>\n"
"<head>\n"
"<title>%s</title>"
;
    
const char* BODY_START = 
"</head>\n"
"<body>\n";

const char* BODY_END = 
"</body>\n"
"</html>\n"
;

/**
 * This was adapted from http://www.w3.org/StyleSheets/TR/W3C-CR.css.
 * I captured it in code so we can build documents that can be displayed
 * without necessarily having an internet connection (e.g. on the plane).
 * The first part is for some reason included literally in the
 * documents, the second part starting with "body {" 
 * is the contents of the URL.
 *
 * Since capture, several changes and simplifications have been made.
 */
const char* W3C_STYLE = 
"<style type='text/css'>\n"
"<!-- \n"
//"code           { font-family: monospace; }"
""
"div.constraint,"
"div.issue,"
"div.note,"
"div.notice     { margin-left: 2em; }"
""
"dt.label       { display: run-in; }"
""
// jsl - originally p had these too, but that's too tight
//"li, p           { margin-top: 0.3em;"
//"margin-bottom: 0.3em; }"
"li           { margin-top: 0.3em; margin-bottom: 0.3em; }"
// jsl - not sure what the default is, but this tightens it up a bit
"p           { margin-top: 0.6em; margin-bottom: 0.6em; }"
""
".diff-chg	{ background-color: orange; }"
".diff-del	{ background-color: red; text-decoration: line-through;}"
".diff-add	{ background-color: lime; }"
""
"table          { empty-cells: show; }"
""
""
"div.exampleInner { margin-left: 1em;"
"                       margin-top: 0em; margin-bottom: 0em}"
"div.exampleOuter {border: 4px double gray;"
"                  margin: 0em; padding: 0em}"
"div.exampleInner { background-color: #d5dee3;"
"                   border-top-width: 4px;"
"                   border-top-style: double;"
"                   border-top-color: #d3d3d3;"
"                   border-bottom-width: 4px;"
"                   border-bottom-style: double;"
"                   border-bottom-color: #d3d3d3;"
"                   padding: 4px; margin: 0em }"
"div.exampleWrapper { margin: 4px }"
"div.exampleHeader { font-weight: bold; margin: 4px}"
""
"body {"
"  padding: 2em 1em 2em 70px;"
"  margin: 0;"
"  font-family: sans-serif;"
"  color: black;"
"  background: white;"
"  background-position: top left;"
"  background-attachment: fixed;"
"  background-repeat: no-repeat;"
"}"
":link { color: #00C; background: transparent }"
":visited { color: #609; background: transparent }"
"a:active { color: #C00; background: transparent }"
""
"a:link img, a:visited img { border-style: none } /* no border on img links */"
""
"a img { color: white; }        /* trick to hide the border in Netscape 4 */"
"@media all {                   /* hide the next rule from Netscape 4 */"
"  a img { color: inherit; }    /* undo the color change above */"
"}"
""
"th, td { /* ns 4 */"
"  font-family: sans-serif;"
"}"
""
"h1, h2, h3, h4, h5, h6 { text-align: left }"
"/* background should be transparent, but WebTV has a bug */"
"h1, h2, h3 { color: #005A9C; background: white }"
"h1 { font: 170% sans-serif }"
"h2 { font: 140% sans-serif }"
"h3 { font: 120% sans-serif }"
"h4 { font: bold 100% sans-serif }"
"h5 { font: italic 100% sans-serif }"
"h6 { font: small-caps 100% sans-serif }"
""
".hide { display: none }"
""
"div.head { margin-bottom: 1em }"
"div.head h1 { margin-top: 2em; clear: both }"
"div.head table { margin-left: 2em; margin-top: 2em }"
""
"p.copyright { font-size: small }"
"p.copyright small { font-size: small }"
""
"@media screen {  /* hide from IE3 */"
"a[href]:hover { background: #ffa }"
"}"
""
// jsl - took this out so pre's can be used inside other things 
// left justified, use something else if you need a margin
//"pre { margin-left: 2em }"
// jsl - this had bee commented out of the original style, but
// its what we had to do above to give paras more air, why?
//"/*"
//"p {"
//"  margin-top: 0.6em;"
//"  margin-bottom: 0.6em;"
//"}"
//"*/"
// jsl - I like glossaries to have more air between elements
"dd { margin-top: 0.3em; margin-bottom: 0.6em }" 
"dt { margin-top: 0; margin-bottom: 0 } /* opera 3.50 */"
"dt { font-weight: bold }"
""
// jsl - bumped the font size a bit so it fits in with inline text better
// I didn't really like font-weight:bold though...
// "pre, code { font-family: monospace } /* navigator 4 requires this */"
"pre, code { font-family:monospace; font-size: medium }"
""
"ul.toc {"
"  list-style: disc;		/* Mac NS has problem with 'none' */"
"  list-style: none;"
"}"
""
"-->\n"
"</style>\n";

/**
 * The original boring style sheet
 */
const char *BORING_STYLE = 
"<style>\n"
"<!-- \n"
".title1 {\n"
"    text-align: center; \n"
"}\n"
".title2 {\n"
"    text-align: center; \n"
"}\n"
".sec1 {\n"
"    font: bold 16pt helvetica, sans-serif;\n"
"}\n"
".sec2 {\n"
"    font: bold 14pt helvetica, sans-serif;\n"
"}\n"
".sec3 {\n"
"    font: bold 12pt helvetica, sans-serif;\n"
"}\n"
".sec4 {\n"
"    font: bold 12pt helvetica, sans-serif;\n"
"}\n"
".computer {\n"
"    font: 12pt fixedsys, sans-serif;\n"
"}\n"
"P {\n"
"    text-align: justify;\n"
" }\n"
"-->\n"
"</style>\n"
"</head>\n"
"<body>\n"
;

/****************************************************************************
 *                                                                          *
 *   						  SECTION NUMBERING                             *
 *                                                                          *
 ****************************************************************************/

#define MAX_LEVELS 10

StringList* gSectionIds = NULL;
int gSectionNumbers[MAX_LEVELS];
int gSectionLevel = -1;

void initSectionNumbers()
{
	gSectionLevel = -1;
}

void enterSection()
{
	gSectionLevel++;
	// make sure it starts out at 0
	gSectionNumbers[gSectionLevel] = 0;
}

void leaveSection()
{
	if (gSectionLevel > 0)
	  gSectionLevel--;
}

void incSectionNumber(void)
{
	if (gSectionLevel >= 0)
	  gSectionNumbers[gSectionLevel] += 1;
}

const char *getSectionNumber(void)
{
	static char secbuf[1024];
	strcpy(secbuf, "");

	for (int i = 0 ; i <= gSectionLevel ; i++) {
		if (i)
		  strcat(secbuf, ".");
		char n[32];
		sprintf(n, "%d", gSectionNumbers[i]);
		strcat(secbuf, n);
	}
	return secbuf;
}

// get a level number suitable for use in an <h*> tag
// we support a maximum of 4 levels, not since we uses CSS classes,
// these wouldn't have to correspond to <h> levels too

#define MAX_CLASS_LEVEL 4

int getSectionLevel()
{
	int level = gSectionLevel + 1;
	if (level > MAX_CLASS_LEVEL)
	  level = MAX_CLASS_LEVEL;
	return level;
}

void walkSectionNumbers(XmlElement *el)
{
	if (!strcmp(el->getName(), "section")) {
		incSectionNumber();

		// leave a section number behind
		const char *secnum = getSectionNumber();
		el->setAttribute("number", secnum);
		el->setAttributeInt("level", gSectionLevel);

		enterSection();
	}
	
	// recurse on children
	for (XmlElement *child = el->getChildElement() ; child ; 
		 child = child->getNextElement())
	  walkSectionNumbers(child);

	if (!strcmp(el->getName(), "section"))
	  leaveSection();
}

void annotateSectionNumbers(XmlDocument *doc)
{
	initSectionNumbers();
	enterSection();
	walkSectionNumbers(doc->getChildElement());

	// leave this in the initialized state, this will lose if <TOC/>
	// is anything other than the first thing after the header, could
	// be smarter about saving state here...

	initSectionNumbers();
	enterSection();
}

/****************************************************************************
 *                                                                          *
 *   						  CONVERSION WALKER                             *
 *                                                                          *
 ****************************************************************************/

typedef void (*ELHANDLER)(XmlElement *el, FILE *fp);

typedef struct {

	const char *name;
	ELHANDLER handler;

} HANDLERMAP;

extern HANDLERMAP gElementHandlers[];

void emitElement(XmlElement *el, FILE *fp)
{
	ELHANDLER handler = NULL;
	const char *elname = el->getName();

	for (int i = 0 ; gElementHandlers[i].name ; i++) {
		if (!strcmp(gElementHandlers[i].name, elname)) {
			handler = gElementHandlers[i].handler;
			break;
		}
	}

	if (handler)
	  (*handler)(el, fp);
	else {
		char msg[256];
		sprintf(msg, "Unknown element '%s'", elname);
		// throw BaseException(ERR_GENERIC, msg);
		printf("%s\n", msg);
	}
}

void emitElementContent(XmlElement *el, FILE *fp)
{
	while (el) {
		emitElement(el, fp);
		el = el->getNextElement();
	}
}

void emitContent(XmlNode *node, FILE *fp)
{
	while (node) {

		XmlElement *el = node->isElement();
		if (el)
		  emitElement(el, fp);
		else {
			XmlPcdata *pcd = node->isPcdata();
			if (pcd && pcd->getText()) {
				// Do it one character at a time so we can handle things
				// that confuse fprintf (like %).  Also gives us a chance
				// to put back important character entities that were lost
				// during parsing.

				for (const char *c = pcd->getText() ; *c ; c++) {
					if (*c == '<')
					  fprintf(fp, "&lt;");
					else if (*c == '>') 
					  fprintf(fp, "&gt;");
					else if (*c == '&')
					  fprintf(fp, "&amp;");
					else
					  fputc(*c, fp);
				}
			}
		}
		// else, comment, or something, ignore

		node = node->getNext();
	}
}

/****************************************************************************
 *                                                                          *
 *   							   EMITTERS                                 *
 *                                                                          *
 ****************************************************************************/

void emitDocument(XmlElement *el, FILE *fp)
{
	// nothing to add, descend into children, but arm the level counters
    gSectionIds = new StringList();
	initSectionNumbers();
	enterSection();
	emitElementContent(el->getChildElement(), fp);
	leaveSection();
}

void walkTOC(XmlElement *el, FILE *fp)
{
	if (!strcmp(el->getName(), "section")) {

		// get precalculated section number
		const char *secnum = el->getAttribute("number");
		const char *slevel = el->getAttribute("level");
		int level = atoi(slevel);

		// locate the section title
		const char *title = NULL;
        const char* id = NULL;
		XmlElement *content = el->getChildElement();
		if (content && !strcmp(content->getName(), "title")) {
            title = content->getContent();
            id = content->getAttribute("id");
        }

		// use &npsp; to insert some padding, I dislike this,
		// but there doesn't seem to be a particularly easy
		// way with tables.

		fprintf(fp, "<tr align=left><th>%s</th><td></td>", secnum);

		if (title != NULL) {
			fprintf(fp, "<td>");
			for (int i = 0 ; i < level ; i++)
			  fprintf(fp, "&nbsp;&nbsp;&nbsp;&nbsp;");

            // prefer internal id if we have one, otherwise literal title
            if (id == NULL) id = title;
			fprintf(fp, "<a href='#%s'>%s</a></td>\n", id, title);

            // also record this in the referenceables list for 
            // resolution verification
            gSectionIds->add(id);
		}
	}

	// recurse on children
	for (XmlElement *child = el->getChildElement() ; child ; 
		 child = child->getNextElement())
	  walkTOC(child, fp);

}

void emitTOC(XmlElement *toc, FILE *fp)
{
	fprintf(fp, "<h1 class=sec1>Contents</h1>\n");
	fprintf(fp, "<table>\n");

	// find the root, and walk from there
	XmlNode *root = toc->getParent();
	while (root && root->getParent())
	  root = root->getParent();
	
	if (root)
	  walkTOC(root->getChildElement(), fp);

	fprintf(fp, "</table>\n");
}

void emitHeading(XmlElement *heading, FILE *fp)
{
	// this is expected to be the first thing under the <document>
	// you might get strange results if it isn't

	if (heading) {
		// process the header fields
		XmlElement *e = heading->findElement("title");
		for ( ; e ; e = e->findNextElement("title")) {
			if (e->getContent())
			  fprintf(fp, "<h1 class=title1>%s</h1>\n", e->getContent());
		}

		e = heading->findElement("author");
		if (e && e->getContent())
		  fprintf(fp, "<h2 class=title2>%s</h2>\n", e->getContent());
		  
		e = heading->findElement("version");
		if (e && e->getContent())
		  fprintf(fp, "<h2 class=title2>%s</h2>\n", e->getContent());
		  
		e = heading->findElement("date");
		if (e && e->getContent())
		  fprintf(fp, "<h2 class=title2>%s</h2>\n", e->getContent());

		fprintf(fp, "<br>\n");
	}
}

void emitSection(XmlElement *section, FILE *fp)
{
	incSectionNumber();

	XmlElement *content = section->getChildElement();

	const char* title = NULL;
    const char* id = NULL;

	if (content && !strcmp(content->getName(), "title")) {
		// there is a title element
		title = content->getContent();
        id = content->getAttribute("id");
        if (id == NULL) 
          id = title;
		content = content->getNextElement();
	}

	// This is also now stored as the "number" attribute on the 
	// <section> element, so we wouldn't have to calculate it again
	// here.  But we're using the section state to control other things
	// like fonts, so we continue to maintain the state machine.

	const char *secnum = getSectionNumber();
	int level = getSectionLevel();

	// also add an anchor around the title

	if (id != NULL)
	  fprintf(fp, "<a name='%s'>\n", id);

    // always emit a section even if no title?
    if (title == NULL) title = "";
	fprintf(fp, "<h%d class=sec%d>%s %s</h%d>\n", 
			level, level, secnum, title, level);

	if (id != NULL)
	  fprintf(fp, "</a>\n");

	// iterate over section contents, emit only elements
	enterSection();
	emitElementContent(content, fp);
	leaveSection();
}

void emitTitle(XmlElement *el, FILE *fp)
{
	// should be at the top of those elements that support them
	// assume that its handled by the container
}

void emitPara(XmlElement *el, FILE *fp)
{
	fprintf(fp, "<p>");
	emitContent(el->getChildren(), fp);
	fprintf(fp, "</p>\n");
	// fprintf(fp, "<br>\n");
}

void emitRef(XmlElement *el, FILE *fp)
{
	if (el->getContent()) {
        char idbuf[1024];

        const char* label = el->getContent();
        const char* id = el->getAttribute("id");
        if (id == NULL)
          id = label;

        //fprintf(stdout, "Looking at %s\n", id);

        // strip newlines so we don't have to be careful
        // about wrapping
        if (LastIndexOf(id, "\n") > 0) {
            FilterString(id, "\n\r", true, idbuf, 1024);
            //fprintf(stdout, "WARNING: filtered newlines out of '%s'\n", idbuf);
            id = idbuf;
        }

		fprintf(fp, "<a href='#%s'>%s</a>", id, label);

        if (!gSectionIds->contains(id)) {
            fprintf(stdout, "WARNING: Unresolved reference %s\n", id);
        }
	}
}

void emitItalics(XmlElement *el, FILE *fp)
{
	fprintf(fp, "<i>");
	emitContent(el->getChildren(), fp);
	fprintf(fp, "</i>");
}

void emitBold(XmlElement *el, FILE *fp)
{
	// should we use <b> or <em> ?
	fprintf(fp, "<b>");
	emitContent(el->getChildren(), fp);
	fprintf(fp, "</b>");
}

void emitBreak(XmlElement *el, FILE *fp)
{
	fprintf(fp, "<br>");
}

void emitUnorderedList(XmlElement *el, FILE *fp)
{
	fprintf(fp, "<ul>\n");
	emitElementContent(el->getChildElement(), fp);
	fprintf(fp, "</ul>\n");
	// fprintf(fp, "<br>\n");
}

void emitOrderedList(XmlElement *el, FILE *fp)
{
	fprintf(fp, "<ol>\n");
	emitElementContent(el->getChildElement(), fp);
	fprintf(fp, "</ol>\n");
	// fprintf(fp, "<br>\n");
}

void emitListItem(XmlElement *el, FILE *fp)
{
	fprintf(fp, "<li>");
	emitContent(el->getChildren(), fp);
	fprintf(fp, "</li>\n");
}

void emitPre(XmlElement *el, FILE *fp)
{
	fprintf(fp, "<pre class=computer>");
	emitContent(el->getChildren(), fp);
	fprintf(fp, "</pre>\n");
}

void emitCommand(XmlElement *el, FILE *fp)
{
	// complex substructure, ignore for now
}

void emitExample(XmlElement *el, FILE *fp)
{
    // should provide auto-numbering of these too
    const char* number = el->getAttribute("number");
    const char* title = el->getAttribute("title");

    fprintf(fp, "<div class='exampleOuter'>\n");
    if (number != NULL || title != NULL) {
        fprintf(fp, "<div class='exampleHead'>");
        if (number != NULL) {
            fprintf(fp, "Example %s: ", number);
        }
        if (title != NULL)
          fprintf(fp, "%s", title);
        fprintf(fp, "</div>\n");
    }

    fprintf(fp, "<div class='exampleInner'>\n");
    fprintf(fp, "<pre>");
    emitContent(el->getChildren(), fp);
    fprintf(fp, "</pre>\n");
    fprintf(fp, "</div></div>\n");
}

void emitGlossary(XmlElement *el, FILE *fp) 
{
	// fprintf(fp, "<table cellspacing=3>\n");
	fprintf(fp, "<table border=1>\n");

	for (XmlElement *gi = el->getChildElement() ; gi ; 
		 gi = gi->getNextElement()) {

		// better be a "gi" 
		if (!strcmp(gi->getName(), "gi")) {

			XmlElement *term = gi->getChildElement();
			if (term) {
				XmlElement *def = term->getNextElement();

				fprintf(fp, "<tr align=left valign=top><th>");
				XmlElement *tc = term->getChildElement();
				emitContent(term->getChildren(), fp);
				fprintf(fp, "</th><td></td>");

				if (def) {
					fprintf(fp, "<td>");
					emitContent(def->getChildren(), fp);
					fprintf(fp, "</td>");
				}
				fprintf(fp, "</tr>\n");
			}
		}
	}

	fprintf(fp, "</table>\n");
}

void emitImage(XmlElement *el, FILE *fp) 
{
    const char* src = el->getAttribute("src");
    const char* caption = el->getAttribute("caption");

    if (src == NULL)
      printf("WARNING: <image> without src attribute\n");
    else {

        // fprintf(fp, "<table cellspacing=3>\n");
        fprintf(fp, "<table><tr><td><img alt='Graphic' src='%s'/></td></tr>", src);
        if (caption != NULL) {
            fprintf(fp, "<tr><td><b><i>%s</i></b></td></tr>", caption);
        }
        fprintf(fp, "</table>\n");
    }
}

XmlWriter* gXmlWriter = NULL;

void emitLiteral(XmlElement *el, FILE *fp)
{
    // would be nice to have a writer variant for FILE*
    if (gXmlWriter == NULL) 
      gXmlWriter = new XmlWriter();

    char* xml = gXmlWriter->exec(el);
    if (xml != NULL) {
        fprintf(fp, "%s", xml);
        fprintf(fp, "\n");
        delete xml;
        
    }
}

/****************************************************************************
 *                                                                          *
 *   							   PREAMBLE                                 *
 *                                                                          *
 ****************************************************************************/

void emitPreamble(XmlDocument *doc, FILE *fp)
{
	// isolate the document title so we can include it in the preamble
	const char *title = "";
	XmlElement *header = doc->findElement("heading");
	if (header) {
		XmlElement *t = header->findElement("title");
		if (t) 
		  title = t->getContent();
	}

	// emit the preamble
    fprintf(fp, HEAD_START, title);
	fprintf(fp, "%s", W3C_STYLE);
    fprintf(fp, "%s", BODY_START);
}

void emitPostamble(XmlDocument *doc, FILE *fp)
{
	fprintf(fp, "%s", BODY_END);
}

/****************************************************************************
 *                                                                          *
 *   							 HANDLER MAP                                *
 *                                                                          *
 ****************************************************************************/

HANDLERMAP gElementHandlers[] = {
	{"document", 	emitDocument},
	{"heading", 	emitHeading},
	{"TOC",		 	emitTOC},
	{"section",		emitSection},
	{"title",		emitTitle},
	{"p",			emitPara},
	{"ref",			emitRef},
	{"i",			emitItalics},
	{"b",			emitBold},
	{"br",			emitBreak},
	{"ul",			emitUnorderedList},
	{"ol",			emitOrderedList},
	{"li",			emitListItem},
	{"pre",			emitPre},
	{"example",		emitExample},
	{"command",		emitCommand},
	{"glossary", 	emitGlossary},
    // common HTML pass through
    {"a",           emitLiteral},
    {"img",         emitLiteral},
    {"table",       emitLiteral},
    {"image",       emitImage},
	{NULL, 			NULL}
};

void convert(XmlDocument *doc, FILE *fp)
{
	// annotate section numbers for the TOC
	annotateSectionNumbers(doc);

	emitPreamble(doc, fp);

	emitElementContent(doc->getChildElement(), fp);

	emitPostamble(doc, fp);
}

/****************************************************************************
 * main
 *
 * Arguments:
 *
 * Returns: 
 *
 * Description: 
 * 
 * Parses the input file into an XmlDocument object, then wanders
 * over the object.
 ****************************************************************************/

void usage()
{
	printf("usage: doc2html <infile> <outfile>\n");
}

int main(int argc, char *argv[])
{
    char inbuf[1024];
    char outbuf[1024];
	XomParser		*p;
	XmlDocument		*xml;
	FILE			*outfp;
	int 			status;

	p = NULL;
	xml = NULL;
	outfp = NULL;
	status = 0;

    if (argc < 2) {
		usage();
		return 1;
    }

	const char *infile = argv[1];
    if (!HasExtension(infile)) {
        sprintf(inbuf, "%s.xml", infile);
        infile = inbuf;
    }

    const char* outfile = NULL;
    if (argc > 2) {
        outfile = argv[2];
        if (!HasExtension(outfile)) {
            sprintf(outbuf, "%s.htm", outfile);
            outfile = outbuf;
        }
    }
    else {
		int ext = LastIndexOf(infile, ".");
        if (ext < 0) {
            sprintf(outbuf, "%s.htm", infile);
            outfile = outbuf;
        }
        else {
            CopyString(infile, outbuf, ext + 1);
            strcat(outbuf, ".htm");
            outfile = outbuf;
        }
    }

	p = new XomParser();
    xml = p->parseFile(infile);
    if (xml == NULL) {
        status = p->getErrorCode();
        printf("XML Parser error: %s\n", p->getError());
    }
    else {
        outfp = fopen(outfile, "w");
        if (!outfp)
          printf("Unable to open output file '%s'\n", outfile);
        else
          convert(xml, outfp);
    }

	delete p;
	delete xml;
    delete gXmlWriter;
	if (outfp) fclose(outfp);

    FlushTrace();
    fflush(stdout);

    return status;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
