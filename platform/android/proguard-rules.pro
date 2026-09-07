# Qt calls into these by name via JNI, so R8 must not rename or remove them.
-keep class org.qtproject.qt.** { *; }
-keep class br.gbmaizol.agape48.SafBridge { *; }
-keepclasseswithmembernames class * { native <methods>; }
