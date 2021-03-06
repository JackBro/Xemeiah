/*
 * xem-jni.cpp
 *
 *  Created on: 26 janv. 2010
 *      Author: francois
 */

#include "xem-jni-dom.h"
#include "xem-jni-classes.h"

#include <Xemeiah/trace.h>
#include <Xemeiah/log.h>
#include <Xemeiah/dom/nodeset.h>
#include <Xemeiah/xprocessor/xprocessor.h>

#include <Xemeiah/auto-inline.hpp>

#if 0
#undef Log
#define Log(...) do{} while(0)
#endif

XemJNI xemJNI;

#if 0
#undef AssertBug

#if 1
#define AssertBug(__cond, ...) { if ( ! ( __cond) ) { \
        Error( "Bug ! " __VA_ARGS__); \
        throwException ( Xem::Exception, "Bug ! " __VA_ARGS__ ); } }
#else
#define AssertBug(__cond,...) { if ( ! (__cond) ) { \
        Error( "Invalid conditions : " __VA_ARGS__); \
        char message[512]; \
        snprintf(message, 512, __VA_ARGS__); \
        ev->Throw(exception2JRuntimeException(ev,  message)); \
        return (void*) NULL; \
}}
#endif
#endif

void
XemJniInit (JNIEnv* ev)
{
    Log("Called XemJniInit with ev=%p\n", ev);
    Log("XemJNI size=%lu\n", sizeof(XemJNI));
}

Xem::String
jstring2XemString (JNIEnv* ev, jstring js)
{
    jboolean isCopy = false;
    const char* utf = ev->GetStringUTFChars(js, &isCopy);
    Xem::String result = Xem::String(utf).copy();
    ev->ReleaseStringUTFChars(js, utf);
    return result;
}

jobject
jDocument2JDocumentFactory (JNIEnv* ev, jobject jDocument)
{
    jobject jDocumentFactory = ev->CallObjectMethod(jDocument, getXemJNI().document.getDocumentFactory(ev));
    return jDocumentFactory;
}

jobject
jXPathEvaluator2JDocumentFactory (JNIEnv* ev, jobject jXPathEvaluator)
{
    jobject jDocumentFactory = ev->GetObjectField(jXPathEvaluator, getXemJNI().xpathEvaluator.documentFactory(ev));
    return jDocumentFactory;
}

Xem::XProcessor*
jDocument2XProcessor (JNIEnv* ev, jobject jDocument)
{
    AssertBug(ev->IsInstanceOf(jDocument, getXemJNI().document.getClass(ev)), "Invalid class for jDocument !");
    Xem::XProcessor* xprocessor = (Xem::XProcessor*) (ev->GetLongField(jDocument,
                                                                       getXemJNI().document.__xprocessorPtr(ev)));
    AssertBug(xprocessor != NULL, "Null XProcessor at jDocument=%p!\n", jDocument);
    return xprocessor;
}

jobject
elementRef2JElement (JNIEnv* ev, jobject jDocument, Xem::ElementRef& eltRef)
{
    jclass clazz = NULL;
    jmethodID constructor = NULL;
    if (eltRef.isComment())
    {
// clazz = ev->FindClass("org/xemeiah/dom/Comment");
        Bug("Not implemented !");
    }
    else if (eltRef.isPI())
    {
// clazz = ev->FindClass("org/xemeiah/dom/ProcessingInstruction");
        Bug("Not implemented !");
    }
    else if (eltRef.isText())
    {
        clazz = getXemJNI().text.getClass(ev);
        constructor = getXemJNI().text.constructor(ev);
    }
    else
    {
        clazz = getXemJNI().element.getClass(ev);
        constructor = getXemJNI().element.constructor(ev);
    }

    AssertBug(clazz != NULL, "Null class !\n");
    AssertBug(constructor != NULL, "Null constructor !\n");

    jlong elementPtr = (jlong) eltRef.getElementPtr();
    jobject jElement = ev->NewObject(clazz, constructor, jDocument, elementPtr);
    Log("New jElement=%p, jDocument=%p (document=%p), elementPtr=%lx, eltRef=%s\n", jElement, jDocument,
        jDocument2Document(ev, jDocument), elementPtr, eltRef.getKey().c_str());
    return jElement;
}

jobject
attributeRef2JAttribute (JNIEnv* ev, jobject documentObject, Xem::AttributeRef& attrRef)
{
    jobject jAttr = ev->NewObject(getXemJNI().attr.getClass(ev), getXemJNI().attr.constructor(ev), documentObject,
                                  attrRef.getElement().getElementPtr(), attrRef.getAttributePtr());

    return jAttr;
}

jobject
createJDocument (JNIEnv* ev, jobject jFactory, Xem::Document* document, Xem::XProcessor* xprocessor)
{
    jobject jDocument = ev->NewObject(getXemJNI().document.getClass(ev), getXemJNI().document.constructor(ev), jFactory,
                                      (jlong) document, (jlong) xprocessor);
    Log("Created JDocument at %p for document %p, xprocessor %p\n", jDocument, document, xprocessor);
    return jDocument;
}

void
cleanupJDocument (JNIEnv *ev, jobject jDocument)
{
    /**
     * Here we consider that cleanup can already have occured
     */
    try
    {
        Log("Cleanup JDocument at %p\n", jDocument);
        Xem::XProcessor* xprocessor = jDocument2XProcessor(ev, jDocument);
        Log("Delete XProcessor at %p\n", xprocessor);
        ev->SetLongField(jDocument, getXemJNI().document.__xprocessorPtr(ev), (jlong) 0);
        delete (xprocessor);

        Xem::Document* doc = jDocument2Document(ev, jDocument);
        Log("Delete document : jDocument=%p, doc=%p\n", jDocument, doc);
        ev->SetLongField(jDocument, getXemJNI().document.__documentPtr(ev), (jlong) 0);

        jobject jDocumentFactory = jDocument2JDocumentFactory(ev, jDocument);
        Xem::Store* store = jDocumentFactory2Store(ev, jDocumentFactory);
        store->releaseDocument(doc);
    }
    catch (Xem::Exception* e)
    {
        Error("Catched exception : %s\n", e->getMessage().c_str());
        ev->Throw(exception2JRuntimeException(ev, e));
    }
}

void
initDocumentFactory (JNIEnv* ev, jobject jFactory, Xem::Store* store)
{
    ev->SetLongField(jFactory, getXemJNI().documentFactory.__storePtr(ev), (jlong) store);
}

Xem::Store*
jDocumentFactory2Store (JNIEnv* ev, jobject jFactory)
{
    AssertBug(jFactory, "Null jFactory provided !");
    jlong __storePtr = ev->GetLongField(jFactory, getXemJNI().documentFactory.__storePtr(ev));
    Xem::Store* store = (Xem::Store*) (__storePtr);
    AssertBug(store, "Null store provided !");
    return store;
}

Xem::Document*
jDocument2Document (JNIEnv* ev, jobject documentObject)
{
    jlong __documentPtr = ev->GetLongField(documentObject, getXemJNI().document.__documentPtr(ev));
    Xem::Document* doc = (Xem::Document*) __documentPtr;
    AssertBug(doc, "Null document !\n");
    return doc;
}

jobject
jNode2JDocument (JNIEnv* ev, jobject nodeObject)
{
    jobject documentObject = ev->GetObjectField(nodeObject, getXemJNI().node.document(ev));
    return documentObject;
}

jobject
jNodeList2JDocument (JNIEnv* ev, jobject jNodeList)
{
    jobject jDocument = ev->GetObjectField(jNodeList, getXemJNI().nodeList.document(ev));
    return jDocument;
}

Xem::String
getJObjectClassname (JNIEnv* ev, jobject jObject)
{
    jclass clazz = ev->GetObjectClass(jObject);
    jobject jName = ev->CallObjectMethod(clazz, getXemJNI().javaLangClass.getName(ev));
    return jstring2XemString(ev, (jstring) jName);
}

bool
isJElement (JNIEnv* ev, jobject jElement)
{
    return (ev->IsInstanceOf(jElement, getXemJNI().element.getClass(ev))
            || ev->IsInstanceOf(jElement, getXemJNI().text.getClass(ev)));
}

Xem::ElementRef
jElement2ElementRef (JNIEnv* ev, jobject jElement)
{
    AssertBug(jElement, "Null object !\n");
    AssertBug(isJElement(ev, jElement), "Incorrect class : %s!\n", getJObjectClassname(ev, jElement).c_str());

    jobject jDocument = jNode2JDocument(ev, jElement);
    AssertBug(jDocument, "Null document object !\n");

    Xem::Document* doc = jDocument2Document(ev, jDocument);

    // Log("jElement=%p, jDocument=%p, doc=%p\n", jElement, jDocument, doc);

    jlong elementPtr = ev->GetLongField(jElement, getXemJNI().node.nodePtr(ev));

    AssertBug(elementPtr, "Null ElementPtr for jElement=%p, jDocument=%p, doc=%p!\n", jElement, jDocument, doc);

    Xem::ElementRef eltRef = Xem::ElementRefConstructor(*doc, elementPtr);
    return eltRef;
}

Xem::AttributeRef
j2AttributeRef (JNIEnv* ev, jobject attrObject)
{
    jobject documentObject = jNode2JDocument(ev, attrObject);
    Xem::Document* doc = jDocument2Document(ev, documentObject);

    jlong nodePtr = ev->GetLongField(attrObject, getXemJNI().attr.__nodePtr(ev));
    jlong attrPtr = ev->GetLongField(attrObject, getXemJNI().attr.__attributePtr(ev));

    return Xem::AttributeRefConstructor(*doc, nodePtr, attrPtr);
}

Xem::KeyId
j2KeyId (JNIEnv* ev, Xem::KeyCache& keyCache, jstring jNamespace, jstring jName)
{
    jboolean isCopy = false;
    const char* nsUrl = jNamespace ? ev->GetStringUTFChars(jNamespace, &isCopy) : NULL;
    const char* localName = ev->GetStringUTFChars(jName, &isCopy);

    Log("j2KeyId : nsUrl=%s, localName=%s\n", nsUrl, localName);

    Xem::NamespaceId nsId = nsUrl ? keyCache.getNamespaceId(nsUrl) : 0;
    Xem::KeyId keyId = keyCache.getKeyId(nsId, localName, false);

    ev->ReleaseStringUTFChars(jNamespace, nsUrl);
    ev->ReleaseStringUTFChars(jName, localName);
    return keyId;
}

jobject
nodeSet2JNamedNodeMap (JNIEnv* ev, jobject jDocument, Xem::NodeSet* result)
{
    jobject nodeListObject = ev->NewObject(getXemJNI().namedNodeMap.getClass(ev),
                                           getXemJNI().namedNodeMap.constructor(ev), jDocument, result->size(), result);
    return nodeListObject;
}

jobject
nodeSet2JNodeList (JNIEnv* ev, jobject jDocument, Xem::NodeSet* result)
{
    jobject nodeListObject = ev->NewObject(getXemJNI().xpathResult.getClass(ev),
                                           getXemJNI().xpathResult.constructor(ev), jDocument, result->size(), result);
    return nodeListObject;
}

Xem::NodeSet*
jNodeList2NodeSet (JNIEnv* ev, jobject jNodeList)
{
    jlong ptr = ev->GetLongField(jNodeList, getXemJNI().nodeList.__nodeListPtr(ev));
    Xem::NodeSet* nodeSet = (Xem::NodeSet*) ptr;
    Log("[ev=%p], jNodeList=%p => nodeList=%p\n", ev, jNodeList, nodeSet);
    return nodeSet;
}

jobject
createJXPathExpression (JNIEnv* ev, Xem::XPathParser* xpathParser, jobject jFactory)
{
    jobject jXPathExpression = ev->NewObject(getXemJNI().xpathExpression.getClass(ev),
                                             getXemJNI().xpathExpression.constructor(ev), (jlong) xpathParser,
                                             jFactory);
    Log("[ev=%p], xpath=%p, jXPath=%p\n", ev, xpathParser, jXPathExpression);
    return jXPathExpression;
}

Xem::XPathParser*
jXPathExpression2XPathParser (JNIEnv* ev, jobject jXPathExpression)
{
    jlong ptr = ev->GetLongField(jXPathExpression, getXemJNI().xpathExpression.__xpathPtr(ev));

    //    AssertBug(dynamic_cast<Xem::XPathParser*>((void*)ptr) != NULL, "Pointer %llx not a valid XPathParser !", ptr);
    Xem::XPathParser* xpath = (Xem::XPathParser*) (ptr);
    Log("[ev=%p], xpath=%p, jXPath=%p\n", ev, xpath, jXPathExpression);
    return xpath;
}

jobject
jXPathExpression2JDocumentFactory (JNIEnv* ev, jobject jXPathExpression)
{
    return ev->GetObjectField(jXPathExpression, getXemJNI().xpathExpression.documentFactory(ev));
}

jthrowable
exception2JRuntimeException (JNIEnv* ev, Xem::Exception* exception)
{
    jstring msg = ev->NewStringUTF(exception->getMessage().c_str());
    jthrowable jException = (jthrowable) ev->NewObject(getXemJNI().javaLangRuntimeException.getClass(ev),
                                      getXemJNI().javaLangRuntimeException.constructor(ev), msg);
    Info("jException=%p\n", jException);
    return jException;
}

jthrowable
exception2JXPathException (JNIEnv* ev, Xem::Exception* exception)
{
    jstring msg = ev->NewStringUTF(exception->getMessage().c_str());
    return (jthrowable) ev->NewObject(getXemJNI().xpathException.getClass(ev),
                                      getXemJNI().xpathException.constructor(ev), (jshort) 0, msg);
}

jthrowable
exception2JDOMException (JNIEnv* ev, const char* message)
{
    jstring msg = ev->NewStringUTF(message);
    return (jthrowable) ev->NewObject(getXemJNI().domException.getClass(ev), getXemJNI().domException.constructor(ev),
                                      (jshort) 0, msg);
}

jthrowable
exception2JDOMException (JNIEnv* ev, Xem::Exception* exception)
{
    return exception2JDOMException(ev, exception->getMessage().c_str());
}

Xem::String
jLookupNamespaceURI (JNIEnv* ev, jobject jNSResolver, Xem::String nsPrefix)
{
    jstring jNsPrefix = ev->NewStringUTF(nsPrefix.c_str());
    jstring result = (jstring) ev->CallObjectMethod(jNSResolver, getXemJNI().xpathNSResolver.lookupNamespaceURI(ev),
                                                    jNsPrefix);
    if (result != NULL)
    {
        return jstring2XemString(ev, result);
    }
    Error("Could not resolve %s !\n", nsPrefix.c_str());
    return "";
}
