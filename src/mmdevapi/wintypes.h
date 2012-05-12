#ifndef WIN32_H
#define WIN32_H

#include <base/types.h>

#define STDAPICALLTYPE		__stdcall
#define DllExport		__declspec( dllexport )
#define WINAPI			DllExport STDAPICALLTYPE

#define CONST			const

typedef uint32_t  HRESULT;
#define SUCCEEDED(hr)		((hr & 0x80000000) == 0)
#define FAILED(hr)		((hr & 0x80000000) != 0)

#define EXTERN_C		extern
#define STDAPICALLTYPE		__stdcall
#define STDAPI			EXTERN_C HRESULT STDAPICALLTYPE

#define ERROR_FILE_NOT_FOUND	2L
#define ERROR_NO_MORE_FILES	18L

#define MAX_PATH		260
#define INVALID_HANDLE_VALUE	((HANDLE) -1)
#define INVALID_FILE_SIZE	((DWORD)0xFFFFFFFF)

#define WAIT_TIMEOUT		0x00000102L
#define WAIT_FAILED		0xFFFFFFFFL

#define FD_SETSIZE		64


#define STGM_READ		0x00000000L

typedef void* HANDLE;
typedef HANDLE* PHANDLE;
typedef HANDLE* LPHANDLE;

typedef void  VOID;
typedef void* PVOID;
typedef const void* LPCVOID;

typedef char TCHAR;
typedef char CHAR;
typedef short SHORT;
typedef unsigned short WCHAR;
typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef unsigned long DWORD;
typedef int INT;
typedef long LONG;
typedef int64_t LONGLONG;
typedef uint64_t ULONGLONG;

typedef unsigned char UCHAR;
typedef unsigned short USHORT;
typedef unsigned int UINT;
typedef unsigned long ULONG;
typedef uint32_t UINT32;

typedef int BOOL;
typedef unsigned int SIZE_T;

typedef float FLOAT;
typedef double DOUBLE;

typedef WCHAR OLECHAR;
typedef OLECHAR* BSTR;
typedef BSTR* LPBSTR;

typedef char* LPSTR;
typedef char* LPTSTR;
typedef const char* LPCTSTR;
typedef const char* LPCSTR;

typedef unsigned short* LPWSTR;
typedef const unsigned short* LPCWSTR;

typedef unsigned short VARTYPE;
typedef short VARIANT_BOOL;
typedef VARIANT_BOOL _VARIANT_BOOL;
typedef LONG SCODE;

typedef double DATE;

typedef BYTE* PBYTE;
typedef BYTE* LPBYTE;
typedef LONG* LPLONG;
typedef LONG* PLONG;
typedef INT* LPINT;
typedef DWORD* LPDWORD;
typedef DWORD* PDWORD;
typedef BOOL* LPBOOL;
typedef void* LPVOID;
// typedef hmodule_t HMODULE;
typedef void* FARPROC;
typedef int LCID;
typedef int HKEY;
typedef HKEY* PHKEY;
typedef int REGSAM;
typedef int HWND;
typedef int MMRESULT;
typedef int TOKEN_INFORMATION_CLASS;

typedef void* LPCRITICAL_SECTION;
typedef void* LPSECURITY_ATTRIBUTES;
typedef void* LPSTARTUPINFO;
typedef void* LPPROCESS_INFORMATION;
typedef void* LPTIME_ZONE_INFORMATION;
typedef void* PINPUT_RECORD;
typedef void* PHANDLER_ROUTINE;
typedef void* LPWSAOVERLAPPED;
typedef void* LPWSAOVERLAPPED_COMPLETION_ROUTINE;
typedef void* PSID;
typedef void* PSID_IDENTIFIER_AUTHORITY;
typedef void* PACL;
typedef void* PSECURITY_DESCRIPTOR;
typedef void* LPSERVICE_TABLE_ENTRY;
typedef void* LPSERVICE_STATUS;
typedef void* HCRYPTPROV;
typedef void* LPOVERLAPPED;

typedef LONGLONG REFERENCE_TIME;
typedef int SERVICE_STATUS_HANDLE;

typedef struct GUID     // 16 bytes
{
    DWORD   Data1;
    WORD    Data2;
    WORD    Data3;
    BYTE    Data4[8];   // chars are one-based
} GUID;

typedef GUID* REFGUID;
typedef GUID  IID;
typedef GUID* REFIID;
typedef GUID  CLSID;
typedef GUID* REFCLSID;
typedef GUID  UUID;
typedef const GUID* LPCGUID;

#ifdef INITGUID
# define DEFINE_GUID(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
        const GUID DECLSPEC_SELECTANY name = \
    { l, w1, w2, { b1, b2,  b3,  b4,  b5,  b6,  b7,  b8 } }
#else
# define DEFINE_GUID(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
    EXTERN_C const GUID name
#endif


typedef struct WAVEFORMATEX {
    WORD wFormatTag;
    WORD nChannels;
    DWORD nSamplesPerSec;
    DWORD nAvgBytesPerSec;
    WORD nBlockAlign;
    WORD wBitsPerSample;
    WORD cbSize;
} WAVEFORMATEX;

typedef union _ULARGE_INTEGER {
  struct {
    DWORD LowPart;
    DWORD HighPart;
  };
  struct {
    DWORD LowPart;
    DWORD HighPart;
  } u;
  ULONGLONG QuadPart;
} ULARGE_INTEGER, *PULARGE_INTEGER;

typedef union _LARGE_INTEGER {
  struct {
    DWORD LowPart;
    LONG  HighPart;
  };
  struct {
    DWORD LowPart;
    LONG  HighPart;
  } u;
  LONGLONG QuadPart;
} LARGE_INTEGER, *PLARGE_INTEGER;

typedef struct _FILETIME
{
  DWORD dwLowDateTime;
  DWORD dwHighDateTime;
} FILETIME, *PFILETIME, *LPFILETIME;

typedef struct _SYSTEMTIME
{
  WORD wYear;
  WORD wMonth;
  WORD wDayOfWeek;
  WORD wDay;
  WORD wHour;
  WORD wMinute;
  WORD wSecond;
  WORD wMilliseconds;
} SYSTEMTIME, *PSYSTEMTIME, *LPSYSTEMTIME;

typedef struct tagCLIPDATA {
    ULONG cbSize;
    long ulClipFmt;
    PBYTE pClipData;
} CLIPDATA;

#define TYPEDEF_CA(type, name) \
    typedef struct tag ## name {\
        ULONG cElems;\
        type *pElems;\
        } name

#ifdef __GNUC__
#ifndef NONAMELESSUNION
#if __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 95) 
#define _ANONYMOUS_UNION __extension__
#define _ANONYMOUS_STRUCT __extension__
#else
#if defined(__cplusplus)
#define _ANONYMOUS_UNION __extension__
#endif
#endif /* __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 95) */
#endif /* NONAMELESSUNION */
#endif /* __GNUC__ */
#ifndef _ANONYMOUS_STRUCT
#define _STRUCT_NAME(x) x
#else
#define _STRUCT_NAME(x)
#endif
typedef union tagCY {
    _ANONYMOUS_STRUCT struct {
        unsigned long Lo;
        long Hi;
    }_STRUCT_NAME(s);
    LONGLONG int64;
} CY;

typedef struct  tagDEC {
    USHORT             wReserved;
    BYTE               scale;
    BYTE               sign;
    ULONG              Hi32;
    ULONGLONG          Lo64;
} DECIMAL;

typedef struct  tagBSTRBLOB {
    ULONG cbSize;
    PBYTE pData;
} BSTRBLOB;

typedef struct _BLOB {
    ULONG    cbSize;
    BYTE    *pBlobData;
} BLOB,*PBLOB,*LPBLOB;

typedef struct tagCABOOL {
    ULONG cElems;
    VARIANT_BOOL *pElems;
} CABOOL;

typedef struct tagCAC {
    ULONG cElems;
    CHAR *pElems;
} CAC;

typedef struct tagCACY {
    ULONG cElems;
    CY *pElems;
} CACY;

typedef struct tagCAUB {
    ULONG cElems;
    UCHAR *pElems;
} CAUB;

typedef struct tagCAI {
    ULONG cElems;
    SHORT *pElems;
} CAI;

typedef struct tagCAUI {
    ULONG cElems;
    USHORT *pElems;
} CAUI;

typedef struct tagCAL {
    ULONG cElems;
    LONG *pElems;
} CAL;

typedef struct tagCAUL {
    ULONG cElems;
    ULONG *pElems;
} CAUL;

typedef struct tagCAH {
    ULONG cElems;
    LARGE_INTEGER *pElems;
} CAH;

typedef struct tagCAUH {
    ULONG cElems;
    ULARGE_INTEGER *pElems;
} CAUH;

typedef struct tagCAFLT {
    ULONG cElems;
    FLOAT *pElems;
} CAFLT;

typedef struct tagCADBL {
    ULONG cElems;
    DOUBLE *pElems;
} CADBL;

typedef struct tagCASCODE {
    ULONG cElems;
    SCODE *pElems;
} CASCODE;

typedef struct tagCADATE {
    ULONG cElems;
    DATE *pElems;
} CADATE;

typedef struct tagCAFILETIME {
    ULONG cElems;
    FILETIME *pElems;
} CAFILETIME;

typedef struct tagCACLIPDATA {
    ULONG cElems;
    CLIPDATA *pElems;
} CACLIPDATA;

typedef struct tagCACLSID {
    ULONG cElems;
    CLSID *pElems;
} CACLSID;

typedef struct tagCABSTR {
    ULONG cElems;
    BSTR *pElems;
} CABSTR;

typedef struct tagCABSTRBLOB {
    ULONG cElems;
    BSTRBLOB *pElems;
} CABSTRBLOB;

typedef struct tagCALPSTR {
    ULONG cElems;
    LPSTR *pElems;
} CALPSTR;

typedef struct tagCALPWSTR {
    ULONG cElems;
    LPWSTR *pElems;
} CALPWSTR;

#ifdef __cplusplus
typedef struct tagVersionedStream {
    GUID guidVersion;
    IStream *pStream;
} VERSIONEDSTREAM;
#endif

typedef struct tagVersionedStream *LPVERSIONEDSTREAM;

struct PROPVARIANT;
typedef struct tagCAPROPVARIANT {
    ULONG cElems;
    struct PROPVARIANT *pElems;
} CAPROPVARIANT;

typedef struct tagSAFEARRAYBOUND {
  ULONG cElements;
  LONG  lLbound;
} SAFEARRAYBOUND, *LPSAFEARRAYBOUND;

typedef struct tagSAFEARRAY {
  USHORT         cDims;
  USHORT         fFeatures;
  ULONG          cbElements;
  ULONG          cLocks;
  PVOID          pvData;
  SAFEARRAYBOUND rgsabound[1];
} SAFEARRAY, *LPSAFEARRAY;

typedef struct PROPVARIANT {
  VARTYPE vt;
  WORD    wReserved1;
  WORD    wReserved2;
  WORD    wReserved3;
  union {
    CHAR              cVal;
    UCHAR             bVal;
    SHORT             iVal;
    USHORT            uiVal;
    LONG              lVal;
    ULONG             ulVal;
    INT               intVal;
    UINT              uintVal;
    LARGE_INTEGER     hVal;
    ULARGE_INTEGER    uhVal;
    FLOAT             fltVal;
    DOUBLE            dblVal;
    VARIANT_BOOL      boolVal;
    SCODE             scode;
    CY                cyVal;
    DATE              date;
    FILETIME          filetime;
    CLSID             *puuid;
    CLIPDATA          *pclipdata;
    BSTR              bstrVal;
    BSTRBLOB          bstrblobVal;
    BLOB              blob;
    LPSTR             pszVal;
    LPWSTR            pwszVal;
#ifdef __cplusplus
    IUnknown          *punkVal;
    IDispatch         *pdispVal;
    IStream           *pStream;
    IStorage          *pStorage;
#endif
    LPVERSIONEDSTREAM pVersionedStream;
    LPSAFEARRAY       parray;
    CAC               cac;
    CAUB              caub;
    CAI               cai;
    CAUI              caui;
    CAL               cal;
    CAUL              caul;
    CAH               cah;
    CAUH              cauh;
    CAFLT             caflt;
    CADBL             cadbl;
    CABOOL            cabool;
    CASCODE           cascode;
    CACY              cacy;
    CADATE            cadate;    CAFILETIME        cafiletime;
    CACLSID           cauuid;
    CACLIPDATA        caclipdata;
    CABSTR            cabstr;
    CABSTRBLOB        cabstrblob;
    CALPSTR           calpstr;
    CALPWSTR          calpwstr;
    CAPROPVARIANT     capropvar;
    CHAR              *pcVal;
    UCHAR             *pbVal;
    SHORT             *piVal;
    USHORT            *puiVal;
    LONG              *plVal;
    ULONG             *pulVal;
    INT               *pintVal;
    UINT              *puintVal;
    FLOAT             *pfltVal;
    DOUBLE            *pdblVal;
    VARIANT_BOOL      *pboolVal;
    DECIMAL           *pdecVal;
    SCODE             *pscode;
    CY                *pcyVal;
    DATE              *pdate;
    BSTR              *pbstrVal;
#ifdef __cplusplus
    IUnknown          **ppunkVal;
    IDispatch         **ppdispVal;
#endif
    LPSAFEARRAY       *pparray;
    struct PROPVARIANT  *pvarVal;
  };
} PROPVARIANT;

typedef struct {
    GUID  fmtid;
    DWORD pid;
} PROPERTYKEY;

#define CP_ACP				0 
#define REFPROPERTYKEY			const PROPERTYKEY * __MIDL_CONST

// typedef VOID (__stdcall *LPHANDLER_FUNCTION)(DWORD fdwControl);

#define EVENTLOG_SUCCESS                0x0000
#define EVENTLOG_ERROR_TYPE             0x0001
#define EVENTLOG_WARNING_TYPE           0x0002
#define EVENTLOG_INFORMATION_TYPE       0x0004
#define EVENTLOG_AUDIT_SUCCESS          0x0008
#define EVENTLOG_AUDIT_FAILURE          0x0010

#define FORMAT_MESSAGE_ALLOCATE_BUFFER 0x00000100
#define FORMAT_MESSAGE_IGNORE_INSERTS  0x00000200
#define FORMAT_MESSAGE_FROM_STRING     0x00000400
#define FORMAT_MESSAGE_FROM_HMODULE    0x00000800
#define FORMAT_MESSAGE_FROM_SYSTEM     0x00001000
#define FORMAT_MESSAGE_ARGUMENT_ARRAY  0x00002000
#define FORMAT_MESSAGE_MAX_WIDTH_MASK  0x000000FF

#define CONTEXT_i386    0x00010000
#define CONTEXT_i486    0x00010000

#define CONTEXT_CONTROL         (CONTEXT_i386 | 0x00000001L)
#define CONTEXT_INTEGER         (CONTEXT_i386 | 0x00000002L)
#define CONTEXT_SEGMENTS        (CONTEXT_i386 | 0x00000004L)
#define CONTEXT_FLOATING_POINT  (CONTEXT_i386 | 0x00000008L)
#define CONTEXT_DEBUG_REGISTERS (CONTEXT_i386 | 0x00000010L)
#define CONTEXT_EXTENDED_REGISTERS  (CONTEXT_i386 | 0x00000020L)

#define CONTEXT_FULL (CONTEXT_CONTROL | CONTEXT_INTEGER | CONTEXT_SEGMENTS)

#define MAXIMUM_SUPPORTED_EXTENSION     512

#define SIZE_OF_80387_REGISTERS      80

typedef struct _FLOATING_SAVE_AREA 
{
  DWORD   ControlWord;
  DWORD   StatusWord;
  DWORD   TagWord;
  DWORD   ErrorOffset;
  DWORD   ErrorSelector;
  DWORD   DataOffset;
  DWORD   DataSelector;
  BYTE    RegisterArea[SIZE_OF_80387_REGISTERS];
  DWORD   Cr0NpxState;
} FLOATING_SAVE_AREA;

typedef FLOATING_SAVE_AREA *PFLOATING_SAVE_AREA;

typedef struct _CONTEXT 
{
  DWORD ContextFlags;

  DWORD   Dr0;
  DWORD   Dr1;
  DWORD   Dr2;
  DWORD   Dr3;
  DWORD   Dr6;
  DWORD   Dr7;

  FLOATING_SAVE_AREA FloatSave;

  DWORD   SegGs;
  DWORD   SegFs;
  DWORD   SegEs;
  DWORD   SegDs;

  DWORD   Edi;
  DWORD   Esi;
  DWORD   Ebx;
  DWORD   Edx;
  DWORD   Ecx;
  DWORD   Eax;

  DWORD   Ebp;
  DWORD   Eip;
  DWORD   SegCs;
  DWORD   EFlags;
  DWORD   Esp;
  DWORD   SegSs;

  BYTE    ExtendedRegisters[MAXIMUM_SUPPORTED_EXTENSION];
} CONTEXT;

typedef CONTEXT *PCONTEXT;
typedef CONTEXT *LPCONTEXT;

#define STATUS_NONCONTINUABLE_EXCEPTION     0xC0000025
#define STATUS_INVALID_DISPOSITION          0xC0000026
#define STATUS_UNWIND                       0xC0000027
#define STATUS_BAD_STACK                    0xC0000028
#define STATUS_INVALID_UNWIND_TARGET        0xC0000029

#define STATUS_GUARD_PAGE_VIOLATION         0x80000001
#define EXCEPTION_DATATYPE_MISALIGNMENT     0x80000002
#define EXCEPTION_ACCESS_VIOLATION          0xC0000005
#define EXCEPTION_ILLEGAL_INSTRUCTION       0xC000001D
#define EXCEPTION_ARRAY_BOUNDS_EXCEEDED     0xC000008C
#define EXCEPTION_INT_DIVIDE_BY_ZERO        0xC0000094
#define EXCEPTION_INT_OVERFLOW              0xC0000095
#define EXCEPTION_STACK_OVERFLOW            0xC00000FD

#define EXCEPTION_EXECUTE_HANDLER           1
#define EXCEPTION_CONTINUE_SEARCH           0
#define EXCEPTION_CONTINUE_EXECUTION        -1

#define EH_NONCONTINUABLE   0x01
#define EH_UNWINDING        0x02
#define EH_EXIT_UNWIND      0x04
#define EH_STACK_INVALID    0x08
#define EH_NESTED_CALL      0x10

#define EXCEPTION_CONTINUABLE        0
#define EXCEPTION_NONCONTINUABLE     EH_NONCONTINUABLE

#define EXCEPTION_MAXIMUM_PARAMETERS 15

typedef struct _EXCEPTION_RECORD 
{
  DWORD ExceptionCode;
  DWORD ExceptionFlags;
  struct _EXCEPTION_RECORD *ExceptionRecord;
  PVOID ExceptionAddress;
  DWORD NumberParameters;
  ULONG *ExceptionInformation[EXCEPTION_MAXIMUM_PARAMETERS];
} EXCEPTION_RECORD, *PEXCEPTION_RECORD;

typedef struct _EXCEPTION_POINTERS
{
  PEXCEPTION_RECORD ExceptionRecord;
  PCONTEXT ContextRecord;
} EXCEPTION_POINTERS, *PEXCEPTION_POINTERS;

// typedef LONG (__stdcall *PTOP_LEVEL_EXCEPTION_FILTER)(PEXCEPTION_POINTERS ExceptionInfo);
// typedef PTOP_LEVEL_EXCEPTION_FILTER LPTOP_LEVEL_EXCEPTION_FILTER;

typedef enum _EXCEPTION_DISPOSITION 
{
  ExceptionContinueExecution,
  ExceptionContinueSearch,
  ExceptionNestedException,
  ExceptionCollidedUnwind
} EXCEPTION_DISPOSITION;

struct _EXCEPTION_FRAME;

typedef EXCEPTION_DISPOSITION (*PEXCEPTION_HANDLER)
(
  struct _EXCEPTION_RECORD *ExceptionRecord, 
  struct _EXCEPTION_FRAME *EstablisherFrame,
  struct _CONTEXT *ContextRecord,
  struct _EXCEPTION_FRAME **DispatcherContext
);

typedef struct _EXCEPTION_FRAME
{
  struct _EXCEPTION_FRAME *prev;
  PEXCEPTION_HANDLER handler;
} EXCEPTION_FRAME, *PEXCEPTION_FRAME;

typedef struct _NESTED_FRAME
{
  EXCEPTION_FRAME frame;
  EXCEPTION_FRAME *prev;
} NESTED_FRAME;

typedef struct _SYSTEM_INFO 
{ 
  union 
  { 
    DWORD  dwOemId; 
    struct 
    { 
      WORD wProcessorArchitecture; 
      WORD wReserved; 
    }; 
  }; 
  DWORD  dwPageSize; 
  LPVOID lpMinimumApplicationAddress; 
  LPVOID lpMaximumApplicationAddress; 
  DWORD *dwActiveProcessorMask; 
  DWORD dwNumberOfProcessors; 
  DWORD dwProcessorType; 
  DWORD dwAllocationGranularity; 
  WORD wProcessorLevel; 
  WORD wProcessorRevision; 
} SYSTEM_INFO, *LPSYSTEM_INFO; 

typedef struct _OSVERSIONINFO
{ 
  DWORD dwOSVersionInfoSize; 
  DWORD dwMajorVersion; 
  DWORD dwMinorVersion; 
  DWORD dwBuildNumber; 
  DWORD dwPlatformId; 
  TCHAR szCSDVersion[128];
} OSVERSIONINFO, *LPOSVERSIONINFO; 

typedef struct _MEMORY_BASIC_INFORMATION 
{
  PVOID BaseAddress; 
  PVOID AllocationBase; 
  DWORD AllocationProtect; 
  SIZE_T RegionSize; 
  DWORD State; 
  DWORD Protect; 
  DWORD Type; 
} MEMORY_BASIC_INFORMATION, *PMEMORY_BASIC_INFORMATION; 

typedef struct _MEMORYSTATUS
{
  DWORD dwLength; 
  DWORD dwMemoryLoad; 
  SIZE_T dwTotalPhys; 
  SIZE_T dwAvailPhys; 
  SIZE_T dwTotalPageFile; 
  SIZE_T dwAvailPageFile; 
  SIZE_T dwTotalVirtual; 
  SIZE_T dwAvailVirtual; 
} MEMORYSTATUS, *LPMEMORYSTATUS; 

#define FILE_ATTRIBUTE_DIRECTORY            0x00000010
#define FILE_ATTRIBUTE_NORMAL               0x00000080

typedef struct WIN32_FIND_DATA 
{
  DWORD dwFileAttributes;
  FILETIME ftCreationTime;
  FILETIME ftLastAccessTime;
  FILETIME ftLastWriteTime;
  DWORD nFileSizeHigh;
  DWORD nFileSizeLow;
  DWORD dwReserved0;
  DWORD dwReserved1;
  CHAR cFileName[MAX_PATH];
  CHAR cAlternateFileName[14];
} WIN32_FIND_DATA, *PWIN32_FIND_DATA, *LPWIN32_FIND_DATA;

typedef struct WIN32_FIND_DATAW 
{
  DWORD dwFileAttributes;
  FILETIME ftCreationTime;
  FILETIME ftLastAccessTime;
  FILETIME ftLastWriteTime;
  DWORD nFileSizeHigh;
  DWORD nFileSizeLow;
  DWORD dwReserved0;
  DWORD dwReserved1;
  WCHAR cFileName[MAX_PATH];
  WCHAR cAlternateFileName[14];
} WIN32_FIND_DATAW, *PWIN32_FIND_DATAW, *LPWIN32_FIND_DATAW;

#define GENERIC_READ                     0x80000000
#define GENERIC_WRITE                    0x40000000
#define GENERIC_EXECUTE                  0x20000000
#define GENERIC_ALL                      0x10000000

#define FILE_SHARE_READ                  0x00000001
#define FILE_SHARE_WRITE                 0x00000002
#define FILE_SHARE_DELETE                0x00000004

#define CREATE_NEW                       1
#define CREATE_ALWAYS                    2
#define OPEN_EXISTING                    3
#define OPEN_ALWAYS                      4
#define TRUNCATE_EXISTING                5

#define FILE_ATTRIBUTE_READONLY          0x00000001  
#define FILE_ATTRIBUTE_NORMAL            0x00000080  
#define FILE_ATTRIBUTE_TEMPORARY         0x00000100  

#define FILE_FLAG_WRITE_THROUGH          0x80000000
#define FILE_FLAG_NO_BUFFERING           0x20000000
#define FILE_FLAG_RANDOM_ACCESS          0x10000000
#define FILE_FLAG_SEQUENTIAL_SCAN        0x08000000
#define FILE_FLAG_DELETE_ON_CLOSE        0x04000000
#define FILE_FLAG_OVERLAPPED             0x40000000

#define FILE_ATTRIBUTE_READONLY          0x00000001  
#define FILE_ATTRIBUTE_HIDDEN            0x00000002  
#define FILE_ATTRIBUTE_SYSTEM            0x00000004  
#define FILE_ATTRIBUTE_DIRECTORY         0x00000010  
#define FILE_ATTRIBUTE_ARCHIVE           0x00000020  
#define FILE_ATTRIBUTE_DEVICE            0x00000040  
#define FILE_ATTRIBUTE_NORMAL            0x00000080  
#define FILE_ATTRIBUTE_TEMPORARY         0x00000100  

#define STD_INPUT_HANDLE                 ((DWORD)-10)
#define STD_OUTPUT_HANDLE                ((DWORD)-11)
#define STD_ERROR_HANDLE                 ((DWORD)-12)

typedef struct _KEY_EVENT_RECORD 
{
  BOOL bKeyDown; 
  WORD wRepeatCount; 
  WORD wVirtualKeyCode; 
  WORD wVirtualScanCode; 
  union 
  { 
    WCHAR UnicodeChar; 
    CHAR  AsciiChar; 
  } uChar; 
  DWORD dwControlKeyState; 
} KEY_EVENT_RECORD; 


#endif

