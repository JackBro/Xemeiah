/*
 * javareader.h
 *
 *  Created on: 27 janv. 2010
 *      Author: francois
 */

#include <Xemeiah/io/bufferedreader.h>
#include <jni.h>

#include "xem-jni-classes.h"

namespace Xem
{
  class JavaReader : public BufferedReader
  {
  protected:
    JNIEnv* env;
    jobject inputStreamObject;

    jbyteArray byteArray;
    jint fillSize;
  public:
    JavaReader(JNIEnv* env_, jobject inputStreamObject_)
    : env(env_), inputStreamObject(inputStreamObject_)
    {
      fillSize = 4096;
      byteArray = env->NewByteArray(fillSize);
    }

    ~JavaReader()
    {
      if ( buffer )
      {
        env->ReleaseByteArrayElements(byteArray,(jbyte*)buffer,0);
      }
      buffer = NULL;
    }

    bool fill ()
    {
      if ( buffer )
        env->ReleaseByteArrayElements(byteArray,(jbyte*)buffer,0);
      buffer = NULL;

      jint readBytes = env->CallIntMethod(inputStreamObject,getXemJNI().javaIoInputStream.read(env), byteArray);

      if ( readBytes < 0 )
        {
          bufferIdx = 0;
          bufferSz = 0;
          Log ( "End of read : readBytes=%d\n", readBytes );
          return false;
        }
      jboolean isCopy = false;
      buffer = (char*) env->GetByteArrayElements(byteArray,&isCopy);

      bufferIdx = 0;
      bufferSz = (__ui64) readBytes;

      Log ( "Read : bufferSz=%lld\n", bufferSz );

      return true;
    }

  };
};
