static char __ide_version__ [] = "V1.016"
#if defined ( QT_NO_DEBUG )
" RELEASE";
#else
" DEBUG";
#endif

char* ideGetVersion()
{
   return __ide_version__;
}