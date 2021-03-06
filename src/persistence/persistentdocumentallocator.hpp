#include <Xemeiah/kern/store.h>
#include <Xemeiah/kern/exception.h>
#include <Xemeiah/persistence/persistentdocumentallocator.h>
#include <Xemeiah/trace.h>

#include <Xemeiah/persistence/format/indirection.h>

#define Log_PDA_AbsolutePage Debug
#define Log_PIP(...) Log("[PIP]" __VA_ARGS__)

// #define __XEM_PERSISTENTDOCUMENTALLOCATOR_PAGEINFOPAGETABLE_PARANOID //< Option : Paranoid checks for PageInfoPageTable

namespace Xem
{

#define KnownFixedSegmentSize(__type) template<> __ui64 __INLINE DocumentAllocator::getFixedSegmentSize<__type> () \
  { return sizeof(__type); }
    KnownFixedSegmentSize(IndirectionHeader)
#undef KnownFixedSegmentSize

    __INLINE RevisionPage*
    PersistentDocumentAllocator::getRevisionPage ()
    {
        AssertBug(revisionPageRef.getPage(), "NULL RevisionPage !!!\n");
        return revisionPageRef.getPage();
    }

    __INLINE BranchRevId
    PersistentDocumentAllocator::getBranchRevId ()
    {
        static const BranchRevId NullBranchRevId =
            { 0, 0 };
        if (!revisionPageRef.getPage())
            return NullBranchRevId;
        return revisionPageRef.getPage()->branchRevId;
    }

    __INLINE void
    PersistentDocumentAllocator::alterRevisionPage ()
    {
#if PARANOID
        AssertBug ( revisionPageRef.getPage(), "NULL revisionPage !\n" );
#endif
#ifdef XEM_MEM_PROTECT_SYS
        getPersistentStore().alterPage ( revisionPageRef.getPage() );
#endif  
    }

    __INLINE void
    PersistentDocumentAllocator::protectRevisionPage ()
    {
#if PARANOID
        AssertBug ( revisionPageRef.getPage(), "NULL revisionPage !\n" );
#endif
#ifdef XEM_MEM_PROTECT_SYS
        getPersistentStore().protectPage ( revisionPageRef.getPage());
#endif  
    }

#if 1
    __INLINE void
    PersistentDocumentAllocator::alterPageInfo (PageInfo& pageInfo)
    {
#ifdef XEM_MEM_PROTECT_SYS
        AssertBug ( &pageInfo, "NULL pageInfo !\n" );
        PageInfoPage* pageInfoPage = (PageInfoPage*) ((__ui64)&pageInfo & PagePtr_Mask);
        getPersistentStore().alterPage(pageInfoPage);
#endif
    }

    __INLINE void
    PersistentDocumentAllocator::protectPageInfo (PageInfo& pageInfo)
    {
#ifdef XEM_MEM_PROTECT_SYS
        AssertBug ( &pageInfo, "NULL pageInfo !\n" );
        PageInfoPage* pageInfoPage = (PageInfoPage*) ((__ui64)&pageInfo & PagePtr_Mask);
        getPersistentStore().protectPage(pageInfoPage);
#endif
    }
#endif

    __INLINE PageInfoPagePtr
    PersistentDocumentAllocator::getPageInfoPagePtr (__ui64 indirectionOffset, bool write)
    {
        PageInfoPagePtr pageInfoPagePtr = fetchPageInfoPagePtr(indirectionOffset, write);
        return pageInfoPagePtr;
    }

    __INLINE AbsolutePageRef<PageInfoPage>
    PersistentDocumentAllocator::doGetPageInfoPage (RelativePagePtr relativePagePtr, __ui64& index, bool write )
    {
        mapMutex.assertLocked();
        AssertBug ( relativePagePtr % PageSize == 0, "Relative page pointer not aligned !\n" );
        /*
         * The index of the page in the PageInfoPage global vector
         */
        __ui64 pageIndex = (relativePagePtr >> InPageBits);

        /*
         * The position of the page in its very own PageInfoPage
         */
        index = pageIndex % PageInfo_pointerNumber;

        /*
         * The index of the first page in the PageInfoPage of the provided page
         */
        __ui64 indirectionOffset = pageIndex - index;

        AbsolutePageRef<PageInfoPage> cached = pageInfoCache.getPageInfoPage(relativePagePtr, write);
        if ( cached.getPagePtr() )
        {
            Log_PIP ( "[%llx] CACHE FOUND (indirectionOffset=%llx, pageIndex=%llx, write=%d) => %llx\n",
            relativePagePtr, indirectionOffset, pageIndex, write, cached.getPagePtr() );
            return cached;
        }
        Log_PIP ( "[%llx] CACHE MISS (indirectionOffset=%llx, pageIndex=%llx, write=%d)\n",
        relativePagePtr, indirectionOffset, pageIndex, write );

        Log_PIP ( "getPageInfoPage(relativePagePtr=%llx) : pageIndex=%llx, index=%llx, indirectionOffset=%llx\n",
        relativePagePtr, pageIndex, index, indirectionOffset );

        /**
         * Fetch the PageInfoPage
         * pageInfoPagePtr is the absolute page pointer for this pageInfoPage
         */
        PageInfoPagePtr pageInfoPagePtr = getPageInfoPagePtr ( indirectionOffset, write );

        AssertBug ( pageInfoPagePtr, "Null pageInfoPagePtr !\n" );

        AbsolutePageRef<PageInfoPage> pageInfoPageRef = getPageInfoPage(pageInfoPagePtr);

        Log_PIP ( "[%llx] CACHE SET (indirectionOffset=%llx, pageIndex=%llx, pageInfoPage=%p, write=%d)\n",
        relativePagePtr, indirectionOffset, pageIndex, pageInfoPageRef.getPage(), write );
        pageInfoCache.putPageInfoPage(relativePagePtr, pageInfoPageRef, write);

#if PARANOID
        AssertBug ( pageInfoPageRef.getPage(), "Null PageInfoPage !!!\n" );
        if ( pageInfoPageRef.getPage()->pageInfo[index].branchRevId.branchId == 0 && ! write )
        {
            Bug ( "Invalid zero branchId on relPageIdx=%llx, pageInfoPage=%p, index=%llx, brid=%llx:%llx, abs=%llx\n",
            relativePagePtr, pageInfoPageRef.getPage(), index,
            _brid(pageInfoPageRef.getPage()->pageInfo[index].branchRevId), pageInfoPageRef.getPage()->pageInfo[index].absolutePagePtr );
        }
#endif
        return pageInfoPageRef;
    }

    __INLINE PageInfo&
    PersistentDocumentAllocator::getPageInfo (AbsolutePageRef<PageInfoPage> &pageInfoPageRef, __ui64 index)
    {
        mapMutex.assertLocked();
        return pageInfoPageRef.getPage()->pageInfo[index];
    }

    __INLINE AbsolutePageRef<IndirectionPage>
    PersistentDocumentAllocator::getIndirectionPage (IndirectionPagePtr indirectionPagePtr)
    {
        AssertBug(__getPageType(indirectionPagePtr) == PageType_Indirection, "Invalid page type !\n");
        return getPersistentStore().getAbsolutePage<IndirectionPage>(indirectionPagePtr & PagePtr_Mask);
    }

    __INLINE AbsolutePageRef<PageInfoPage>
    PersistentDocumentAllocator::getPageInfoPage (AbsolutePagePtr pageInfoPagePtr)
    {
        AssertBug(__getPageType(pageInfoPagePtr) == PageType_PageInfo, "Invalid page type !\n");
        return getPersistentStore().getAbsolutePage<PageInfoPage>(pageInfoPagePtr & PagePtr_Mask);
    }

    __INLINE AbsolutePageRef<PageList>
    PersistentDocumentAllocator::getPageList (AbsolutePagePtr pageListPtr)
    {
        return getPersistentStore().getAbsolutePage<PageList>(pageListPtr & PagePtr_Mask);
    }

    __INLINE AbsolutePageRef<RevisionPage>
    PersistentDocumentAllocator::getRevisionPage (AbsolutePagePtr revisionPagePtr)
    {
        return getPersistentStore().getAbsolutePage<RevisionPage>(revisionPagePtr & PagePtr_Mask);
    }

    __INLINE AbsolutePageRef<SegmentPage>
    PersistentDocumentAllocator::getSegmentPage (AbsolutePagePtr absPagePtr)
    {
        AssertBug(__getPageType(absPagePtr) == PageType_Segment, "Invalid page type !\n");
        return getPersistentStore().getAbsolutePage<SegmentPage>(absPagePtr & PagePtr_Mask);
    }

}
;
