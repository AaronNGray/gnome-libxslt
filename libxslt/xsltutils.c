/*
 * xsltutils.c: Utilities for the XSL Transformation 1.0 engine
 *
 * Reference:
 *   http://www.w3.org/TR/1999/REC-xslt-19991116
 *
 * See Copyright for the status of this software.
 *
 * Daniel.Veillard@imag.fr
 */

#include "xsltconfig.h"

#include <stdio.h>
#include <stdarg.h>

#include <libxml/xmlversion.h>
#include <libxml/xmlmemory.h>
#include <libxml/tree.h>
#include <libxml/xmlerror.h>
#include <libxml/xmlIO.h>
#include "xsltutils.h"
#include "xsltInternals.h"


/************************************************************************
 * 									*
 * 		Handling of out of context errors			*
 * 									*
 ************************************************************************/

/**
 * xsltGenericErrorDefaultFunc:
 * @ctx:  an error context
 * @msg:  the message to display/transmit
 * @...:  extra parameters for the message display
 * 
 * Default handler for out of context error messages.
 */
void
xsltGenericErrorDefaultFunc(void *ctx, const char *msg, ...) {
    va_list args;

    if (xsltGenericErrorContext == NULL)
	xsltGenericErrorContext = (void *) stderr;

    va_start(args, msg);
    vfprintf((FILE *)xsltGenericErrorContext, msg, args);
    va_end(args);
}

xmlGenericErrorFunc xsltGenericError = xsltGenericErrorDefaultFunc;
void *xsltGenericErrorContext = NULL;


/**
 * xsltSetGenericErrorFunc:
 * @ctx:  the new error handling context
 * @handler:  the new handler function
 *
 * Function to reset the handler and the error context for out of
 * context error messages.
 * This simply means that @handler will be called for subsequent
 * error messages while not parsing nor validating. And @ctx will
 * be passed as first argument to @handler
 * One can simply force messages to be emitted to another FILE * than
 * stderr by setting @ctx to this file handle and @handler to NULL.
 */
void
xsltSetGenericErrorFunc(void *ctx, xmlGenericErrorFunc handler) {
    xsltGenericErrorContext = ctx;
    if (handler != NULL)
	xsltGenericError = handler;
    else
	xsltGenericError = xsltGenericErrorDefaultFunc;
}

/**
 * xsltGenericDebugDefaultFunc:
 * @ctx:  an error context
 * @msg:  the message to display/transmit
 * @...:  extra parameters for the message display
 * 
 * Default handler for out of context error messages.
 */
void
xsltGenericDebugDefaultFunc(void *ctx, const char *msg, ...) {
    va_list args;

    if (xsltGenericDebugContext == NULL)
	return;

    va_start(args, msg);
    vfprintf((FILE *)xsltGenericDebugContext, msg, args);
    va_end(args);
}

xmlGenericErrorFunc xsltGenericDebug = xsltGenericDebugDefaultFunc;
void *xsltGenericDebugContext = NULL;


/**
 * xsltSetGenericDebugFunc:
 * @ctx:  the new error handling context
 * @handler:  the new handler function
 *
 * Function to reset the handler and the error context for out of
 * context error messages.
 * This simply means that @handler will be called for subsequent
 * error messages while not parsing nor validating. And @ctx will
 * be passed as first argument to @handler
 * One can simply force messages to be emitted to another FILE * than
 * stderr by setting @ctx to this file handle and @handler to NULL.
 */
void
xsltSetGenericDebugFunc(void *ctx, xmlGenericErrorFunc handler) {
    xsltGenericDebugContext = ctx;
    if (handler != NULL)
	xsltGenericDebug = handler;
    else
	xsltGenericDebug = xsltGenericDebugDefaultFunc;
}

/************************************************************************
 * 									*
 * 				Sorting					*
 * 									*
 ************************************************************************/

/**
 * xsltSortFunction:
 * @list:  the node set
 * @results:  the results
 * @descending:  direction of order
 * @number:  the type of the result
 *
 * reorder the current node list @list accordingly to the values
 * present in the array of results @results
 */
void	
xsltSortFunction(xmlNodeSetPtr list, xmlXPathObjectPtr *results,
		 int descending, int number) {
    int i, j;
    int len, tst;
    xmlNodePtr node;
    xmlXPathObjectPtr tmp;

    if ((list == NULL) || (results == NULL))
	return;
    len = list->nodeNr;
    if (len <= 1)
	return;
    /* TODO: sort is really not optimized, does it needs to ? */
    for (i = 0;i < len -1;i++) {
	for (j = i + 1; j < len; j++) {
	    if (results[i] == NULL)
		tst = 0;
	    else if (results[j] == NULL)
		tst = 1;
	    else if (number) {
		tst = (results[i]->floatval > results[j]->floatval);
		if (descending)
		    tst = !tst;
	    } else {
		tst = xmlStrcmp(results[i]->stringval, results[j]->stringval);
		if (descending)
		    tst = !tst;
	    }
	    if (tst) {
		tmp = results[i];
		results[i] = results[j];
		results[j] = tmp;
		node = list->nodeTab[i];
		list->nodeTab[i] = list->nodeTab[j];
		list->nodeTab[j] = node;
	    }
	}
    }
}

/************************************************************************
 * 									*
 * 				Output					*
 * 									*
 ************************************************************************/

/**
 * xsltSaveResultTo:
 * @buf:  an output buffer
 * @result:  the result xmlDocPtr
 * @style:  the stylesheet
 *
 * Save the result @result obtained by applying the @style stylesheet
 * to an I/O output channel @buf
 *
 * Returns the number of byte written or -1 in case of failure.
 */
int
xsltSaveResultTo(xmlOutputBufferPtr buf, xmlDocPtr result,
	       xsltStylesheetPtr style) {
    const xmlChar *encoding;
    xmlNodePtr root;
    int base;

    if ((buf == NULL) || (result == NULL) || (style == NULL))
	return(-1);

    if (style->methodURI != NULL) {
        xsltGenericError(xsltGenericErrorContext,
		"xsltSaveResultTo : unknown ouput method\n");
        return(-1);
    }

    /* TODO: when outputing and having imported stylesheets, apply cascade */
    base = buf->written;
    encoding = style->encoding;
    if (style->method == NULL)
	root = xmlDocGetRootElement(result);
    else
	root = NULL;
    if (((style->method != NULL) &&
	 (xmlStrEqual(style->method, (const xmlChar *) "html"))) ||
	((root != NULL) &&
	 (xmlStrEqual(root->name, (const xmlChar *) "html")))){
#if LIBXML_VERSION > 20211
	htmlDocContentDumpOutput(buf, result, (const char *) encoding);
#else
	xsltGenericError(xsltGenericErrorContext,
		"HTML output requires libxml version > 2.2.11\n");
#endif
    } else if ((style->method != NULL) &&
	       (xmlStrEqual(style->method, (const xmlChar *) "text"))) {
	xmlNodePtr cur;

	cur = result->children;
	while (cur != NULL) {
	    if (cur->type == XML_TEXT_NODE)
		xmlOutputBufferWriteString(buf, (const char *) cur->content);
	    cur = cur->next;
	}
    } else {
	if (style->omitXmlDeclaration != 1) {
	    xmlOutputBufferWriteString(buf, "<?xml version=");
	    if (result->version != NULL) 
		xmlBufferWriteQuotedString(buf->buffer, result->version);
	    else
		xmlOutputBufferWriteString(buf, "\"1.0\"");
	    if (encoding == NULL) {
		if (result->encoding != NULL)
		    encoding = result->encoding;
		else if (result->charset != XML_CHAR_ENCODING_UTF8)
		    encoding = (const xmlChar *)
			       xmlGetCharEncodingName((xmlCharEncoding)
			                              result->charset);
	    }
	    if (encoding != NULL) {
		xmlOutputBufferWriteString(buf, " encoding=");
		xmlBufferWriteQuotedString(buf->buffer, (xmlChar *) encoding);
	    }
	    switch (style->standalone) {
		case 0:
		    xmlOutputBufferWriteString(buf, " standalone=\"no\"");
		    break;
		case 1:
		    xmlOutputBufferWriteString(buf, " standalone=\"yes\"");
		    break;
		default:
		    break;
	    }
	    xmlOutputBufferWriteString(buf, "?>\n");
	}
	if (result->children != NULL) {
	    xmlNodePtr child = result->children;

	    while (child != NULL) {
		xmlNodeDumpOutput(buf, result, child, 0, (style->indent == 1),
			          (const char *) encoding);
		xmlOutputBufferWriteString(buf, "\n");
		child = child->next;
	    }
	}
	xmlOutputBufferFlush(buf);
    }
    return(buf->written - base);
}

/**
 * xsltSaveResultToFilename:
 * @URL:  a filename or URL
 * @result:  the result xmlDocPtr
 * @style:  the stylesheet
 * @compression:  the compression factor (0 - 9 included)
 *
 * Save the result @result obtained by applying the @style stylesheet
 * to a file or URL @URL
 *
 * Returns the number of byte written or -1 in case of failure.
 */
int
xsltSaveResultToFilename(const char *URL, xmlDocPtr result,
			 xsltStylesheetPtr style, int compression) {
    xmlOutputBufferPtr buf;
    int ret;

    if ((URL == NULL) || (result == NULL) || (style == NULL))
	return(-1);

    buf = xmlOutputBufferCreateFilename(URL, NULL, compression);
    if (buf == NULL)
	return(-1);
    xsltSaveResultTo(buf, result, style);
    ret = xmlOutputBufferClose(buf);
    return(ret);
}

/**
 * xsltSaveResultToFile:
 * @file:  a FILE * I/O
 * @result:  the result xmlDocPtr
 * @style:  the stylesheet
 *
 * Save the result @result obtained by applying the @style stylesheet
 * to an open FILE * I/O.
 * This does not close the FILE @file
 *
 * Returns the number of byte written or -1 in case of failure.
 */
int
xsltSaveResultToFile(FILE *file, xmlDocPtr result, xsltStylesheetPtr style) {
    xmlOutputBufferPtr buf;
    int ret;

    if ((file == NULL) || (result == NULL) || (style == NULL))
	return(-1);

    buf = xmlOutputBufferCreateFile(file, NULL);
    if (buf == NULL)
	return(-1);
    xsltSaveResultTo(buf, result, style);
    ret = xmlOutputBufferClose(buf);
    return(ret);
}

/**
 * xsltSaveResultToFd:
 * @fd:  a file descriptor
 * @result:  the result xmlDocPtr
 * @style:  the stylesheet
 *
 * Save the result @result obtained by applying the @style stylesheet
 * to an open file descriptor
 * This does not close the descriptor.
 *
 * Returns the number of byte written or -1 in case of failure.
 */
int
xsltSaveResultToFd(int fd, xmlDocPtr result, xsltStylesheetPtr style) {
    xmlOutputBufferPtr buf;
    int ret;

    if ((fd < 0) || (result == NULL) || (style == NULL))
	return(-1);

    buf = xmlOutputBufferCreateFd(fd, NULL);
    if (buf == NULL)
	return(-1);
    xsltSaveResultTo(buf, result, style);
    ret = xmlOutputBufferClose(buf);
    return(ret);
}
