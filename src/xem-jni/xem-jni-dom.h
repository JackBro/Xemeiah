/*
 * xem-jni.h
 *
 *  Created on: 26 janv. 2010
 *      Author: francois
 */

#ifndef XEMJNI_H_
#define XEMJNI_H_

#include <Xemeiah/kern/document.h>
#include <Xemeiah/dom/elementref.h>
#include <Xemeiah/dom/attributeref.h>
#include <Xemeiah/kern/exception.h>

#include <jni.h>

void
XemJniInit (JNIEnv* ev);

Xem::String
jstring2XemString (JNIEnv* ev, jstring js);

jobject
jDocument2JDocumentFactory (JNIEnv* ev, jobject jDoc);

Xem::XProcessor*
jDocumentFactory2XProcessor (JNIEnv* ev, jobject jDocumentFactory);

jobject
elementRef2JElement (JNIEnv* ev, jobject documentObject, Xem::ElementRef& eltRef);
jobject
attributeRef2J (JNIEnv* ev, jobject documentObject, Xem::AttributeRef& attrRef);

jobject
jNode2JDocument (JNIEnv* ev, jobject nodeObject);
jobject
jNodeList2JDocument (JNIEnv* ev, jobject nodeObject);

jobject
document2JDocument (JNIEnv* ev, jobject jFactory, Xem::Document* document);

Xem::Store*
jDocumentFactory2Store (JNIEnv* ev, jobject jFactory);
Xem::Document*
jDocument2Document (JNIEnv* ev, jobject documentObject);
Xem::ElementRef
jElement2ElementRef (JNIEnv* ev, jobject jElement);
Xem::AttributeRef
j2AttributeRef (JNIEnv* ev, jobject elementObject);

Xem::KeyId
j2KeyId (JNIEnv* ev, Xem::KeyCache& keyCache, jstring jNamespace, jstring jName);

jobject
nodeSet2JNodeList (JNIEnv* ev, jobject documentObject, Xem::NodeSet* result);

Xem::NodeSet*
jNodeList2NodeSet ( JNIEnv* ev, jobject jNodeList);

//jobject
//jXPathExpression2ParsedBytes (JNIEnv* ev, jobject jXPathExpression);

jobject
xpath2JXPathExpression (JNIEnv* ev, Xem::XPath* xpath);

Xem::XPath*
jXPathExpression2XPath (JNIEnv* ev, jobject jXPathExpression);


#endif /* XEMJNI_H_ */
