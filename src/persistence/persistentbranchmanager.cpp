#include <Xemeiah/persistence/persistentbranchmanager.h>
#include <Xemeiah/persistence/persistentstore.h>
#include <Xemeiah/persistence/persistentdocument.h>

#include <Xemeiah/kern/servicemanager.h>

#include <Xemeiah/auto-inline.hpp>
#include <Xemeiah/persistence/auto-inline.hpp>

#define Log_PBM Log

namespace Xem
{
    PersistentBranchManager::PersistentBranchManager (PersistentStore& _persistentStore) :
            persistentStore(_persistentStore)
    {
        buildBranchInfos();
        getStore().getServiceManager().registerService("Persistent Branch Manager Service", this);
    }

    PersistentBranchManager::~PersistentBranchManager ()
    {

    }

    CacheBranchManager::BranchInfo*
    PersistentBranchManager::referenceBranch (BranchId branchId, const BranchRevId& forkedFrom)
    {
        Bug("Don't use referenceBranch directly !!!\n");
        return NULL;
    }

    CacheBranchManager::BranchInfo*
    PersistentBranchManager::referenceBranch (BranchPagePtr branchPagePtr, BranchPage* branchPage)
    {
        AssertBug(branchPage->branchId, "Invalid null branchId for branchPage at %p\n", branchPage);
        AssertBug(branchMap.find ( branchPage->branchId ) == branchMap.end(), "Branch '%llx' already exist !\n",
                  branchPage->branchId);
        BranchInfo* branchInfo = new PersistentBranchInfo(*this, branchPagePtr, branchPage);
        branchMap[branchPage->branchId] = branchInfo;
        branchLookup.put(branchPage->branchId, strdup(branchPage->name));
        return branchInfo;
    }

    void
    PersistentBranchManager::buildBranchInfos ()
    {
        /*
         * Step one : create all branchInfos
         */
        AbsolutePageRef<BranchPage> branchPageRef(&getPersistentStore(), (AbsolutePagePtr) 0);
        for (BranchPagePtr branchPagePtr = getPersistentStore().getSB()->lastBranch; branchPagePtr; branchPagePtr =
                branchPageRef.getPage()->lastBranch)
        {
            branchPageRef = getPersistentStore().getAbsolutePage<BranchPage>(branchPagePtr);

            Log_PBM ( "BranchInfo : branchId=%llx, branchPagePtr=%llx, branchName='%s'\n",
                    branchPageRef.getPage()->branchId, branchPagePtr, branchPageRef.getPage()->name );

            referenceBranch(branchPagePtr, branchPageRef.getPage());

            RevisionPagePtr revisionPagePtr = branchPageRef.getPage()->lastRevisionPage;
            AbsolutePageRef<RevisionPage> revisionPageRef = getPersistentStore().getAbsolutePage<RevisionPage>(revisionPagePtr);

            if ( revisionPageRef.getPage() == NULL )
            {
                Warn("At branch='%s' (branchId=%llx), revisionPagePtr=%llx has no absolutePage !\n",
                     branchPageRef.getPage()->name, branchPageRef.getPage()->branchId, revisionPagePtr);
                continue;
            }

            if (revisionPageRef.getPage()->documentAllocationHeader.writable)
            {
                Warn("At branch='%s' (branchId=%llx), revisionId=%llx, last revision is marked writable !\n",
                     branchPageRef.getPage()->name, branchPageRef.getPage()->branchId, revisionPageRef.getPage()->branchRevId.revisionId);

            }
        }

        /*
         * Step two : update branch dependency
         */
        for (BranchMap::iterator iter = branchMap.begin(); iter != branchMap.end(); iter++)
        {
            BranchInfo* branchInfo = iter->second;
            if (branchInfo->getForkedFrom().branchId)
            {
                Log_PBM ( "Marking dependency branch=%llx -> %llx:%llx\n",
                        branchInfo->getBranchId(), _brid(branchInfo->getForkedFrom() ) );
                markBranchDependency ( branchInfo );
            }
        }
    }

    BranchId
    PersistentBranchManager::createBranch (const String& branchName, BranchRevId forkedFrom, BranchFlags branchFlags)
    {
        Lock lock(branchManagerMutex);
        return doCreateBranch(branchName, forkedFrom, branchFlags);
    }

    BranchId
    PersistentBranchManager::doCreateBranch (const String& _branchName, BranchRevId forkedFrom, BranchFlags branchFlags)
    {
        assertIsLocked();

        const char* branchName = _branchName.c_str();
        char* duplicatedBranchName = NULL;
        if (strlen(branchName) >= BranchPage::name_length)
        {
            Error("Branch Name too long : '%s'\n", branchName);
            return NullPage;
        }

        if (getBranchId(branchName))
        {
            Error("Branch '%s' already exist !\n", branchName);
            if (branchFlags & BranchFlags_MayIndexNameIfDuplicate)
            {
                duplicatedBranchName = (char*) malloc(strlen(branchName) + 32);

                for (__ui64 idx = 1; idx < 1024; idx++)
                {
                    sprintf(duplicatedBranchName, "%s #%llu", branchName, idx);
                    if (!getBranchId(duplicatedBranchName))
                        break;
                }
                branchName = duplicatedBranchName;
            }
            else
            {
                throwException(Exception, "Branch '%s' already exist !\n", branchName);
                return NullPage;
            }
        }
        SuperBlock *sb = getPersistentStore().getSB();
        AbsolutePagePtr branchPagePtr = getPersistentStore().getFreePagePtr();
        AbsolutePageRef<BranchPage> branchPageRef = getPersistentStore().getAbsolutePage<BranchPage>(branchPagePtr);
        Log_PBM ( "New branch for '%s' at page=%p\n", branchName, branchPageRef.getPage() );
        getPersistentStore().alterPage(branchPageRef.getPage());

        strncpy(branchPageRef.getPage()->name, branchName, branchPageRef.getPage()->name_length);
        branchPageRef.getPage()->forkedFrom = forkedFrom;
        branchPageRef.getPage()->lastRevisionPage = NullPage;

        branchPageRef.getPage()->branchFlags = branchFlags;

        getPersistentStore().lockSB();
        branchPageRef.getPage()->branchId = sb->nextBranchId;
        sb->nextBranchId++;

        branchPageRef.getPage()->lastBranch = sb->lastBranch;
        sb->lastBranch = branchPagePtr;

        getPersistentStore().syncPage(branchPageRef.getPage());

        getPersistentStore().protectPage(branchPageRef.getPage());

        getPersistentStore().unlockSB();

        BranchInfo* branchInfo = referenceBranch(branchPagePtr, branchPageRef.getPage());

        if (forkedFrom.branchId)
        {
            markBranchDependency(branchInfo);
        }
        else
        {
            /**
             * Create a new branch from void
             */
#if 0
            branchInfo->lockWrite ();
            RevisionPagePtr revPagePtr = createRevision ( branchPage, true );
            PersistentDocument* revision = instanciateTemporaryPersistentDocument ( revPagePtr );
            revision->commit ();
#endif
        }

        Log_PBM ( "New branch '%s' branchId=%llx, branchPagePtr=%llx, forkedFrom=%llx:%llx\n",
                branchName, branchPageRef.getPage()->branchId, branchPagePtr, _brid(forkedFrom) );

        if (duplicatedBranchName)
        {
            free(duplicatedBranchName);
        }

        return branchPageRef.getPage()->branchId;
    }

    void
    PersistentBranchManager::renameBranch (BranchId branchId, const String& branchName)
    {
        NotImplemented("Not Implemented : renameBranch !");
    }

    void
    PersistentBranchManager::scheduleBranchForRemoval (BranchId branchId)
    {
        lockBranchManager();
        BranchInfo* branchInfo = branchMap[branchId];
        AssertBug(branchInfo, "Null branchInfo !\n");

        branchInfo->setScheduledForRemoval();

        if (branchInfo->isFreeFromDocuments())
        {
            // Bug ( "." );
            // NotImplemented ( "BranchInfo has no instanciated documents !\n" );
            Warn("NOT IMPLEMENTED : BranchInfo has no instanciated documents !\n");
        }
        unlockBranchManager();
    }

    void
    PersistentBranchManager::deleteBranch (BranchInfo* _branchInfo)
    {
        PersistentBranchInfo* branchInfo = dynamic_cast<PersistentBranchInfo*>(_branchInfo);

        AssertBug(branchInfo, "NULL branchInfo.\n");

        AssertBug(branchInfo->isFreeFromDocuments(), "Still have %llu documents set !\n",
                  branchInfo->getInstanciatedDocumentsNumber());

        while (!branchInfo->getDependantBranchInfos().empty())
        {
            BranchInfo* firstBranchInfo = branchInfo->getDependantBranchInfos().front();
            deleteBranch(firstBranchInfo);
        }

        /**
         * Flush our reusable document stack
         */
        branchInfo->cleanupReusableDocument();

        Log_PBM ( "deleteBranch %llx : dependant=%lu branches\n",
                branchInfo->getBranchId(), (unsigned long) branchInfo->getDependantBranchInfos().size() );

        BranchPagePtr branchPagePtr = branchInfo->getBranchPagePtr();
        BranchPage* branchPage = branchInfo->getBranchPage();

        /*
         * First, delete the in-memory structures for this branch
         */
        BranchMap::iterator iter = branchMap.find(branchPage->branchId);

        AssertBug(iter != branchMap.end(), "Could not find branchInfo in branchMap !\n");

        branchMap.erase(iter);

        branchLookup.remove(branchPage->branchId, branchPage->name);

        if (branchInfo->getDependingBranchInfo())
        {
            branchInfo->getDependingBranchInfo()->getDependantBranchInfos().remove(branchInfo);
        }

        delete (branchInfo);

        /*
         * Then, delete on-disk structure : first, check if the branch has remaining revisions...
         */
        if (branchPage->lastRevisionPage)
        {
            dropRevisions(branchPage, 0, 0);
        }

        /*
         * Finally, update the linked-list of branches
         */
        getPersistentStore().lockSB();

        if (getPersistentStore().getSB()->lastBranch == branchPagePtr)
        {
            getPersistentStore().getSB()->lastBranch = branchPage->lastBranch;
        }
        else
        {
            AbsolutePagePtr previousBranchPagePtr = getPersistentStore().getSB()->lastBranch;
            while (previousBranchPagePtr)
            {
                AbsolutePageRef<BranchPage> previousBranchPageRef = getPersistentStore().getAbsolutePage<BranchPage>(
                        previousBranchPagePtr);

                if (previousBranchPageRef.getPage()->lastBranch == branchPagePtr)
                {
                    getPersistentStore().alterPage(previousBranchPageRef.getPage());
                    previousBranchPageRef.getPage()->lastBranch = branchPage->lastBranch;
                    getPersistentStore().protectPage(previousBranchPageRef.getPage());

                    getPersistentStore().syncPage(previousBranchPageRef.getPage());

                    break;
                }
                AbsolutePagePtr nextBranchPtr = previousBranchPageRef.getPage()->lastBranch;
                previousBranchPagePtr = nextBranchPtr;
            }
        }
        getPersistentStore().unlockSB();

        getPersistentStore().alterPage(branchPage);

        memset(branchPage, 0, PageSize);
        getPersistentStore().protectPage(branchPage);

        getPersistentStore().freePage(branchPagePtr);
        getPersistentStore().checkAllContents();
    }

    BranchId
    PersistentBranchManager::getBranchId (const String& branchName)
    {
        return branchLookup.get(branchName.c_str());
    }

    String
    PersistentBranchManager::getBranchName (BranchId branchId)
    {
        BranchInfo* branchInfo = branchMap[branchId];
        AssertBug(branchInfo, "Null branchInfo !\n");
        return String(branchInfo->getName());
    }

    BranchPage*
    PersistentBranchManager::getBranchPage (BranchId branchId)
    {
        PersistentBranchInfo* branchInfo = dynamic_cast<PersistentBranchInfo*>(branchMap[branchId]);
        AssertBug(branchInfo, "Null branchInfo for branchId=%llx\n", branchId);
        return branchInfo->getBranchPage();
    }

    BranchRevId
    PersistentBranchManager::getForkedFrom (BranchId branchId)
    {
        BranchInfo* branchInfo = branchMap[branchId];
        AssertBug(branchInfo, "Null branchInfo for branchId=%llx\n", branchId);
        return branchInfo->getForkedFrom();
    }

    RevisionPagePtr
    PersistentBranchManager::lookupRevision (BranchPage* branchPage, RevisionId& revisionId)
    {
        AbsolutePageRef<RevisionPage> revisionPageRef(getPersistentStore());
        if (revisionId == 0)
        {
            if (branchPage->lastRevisionPage == NullPage)
            {
                throwException(Exception, "Branch '%s' (branchId=%llx) has no revision !\n", branchPage->name,
                               branchPage->branchId);
            }
            revisionPageRef = getPersistentStore().getAbsolutePage<RevisionPage>(branchPage->lastRevisionPage);
            AssertBug(revisionPageRef.getPage(), "No Revision Page ! branchPage->lastRevisionPage=%llx\n",
                      branchPage->lastRevisionPage);
            revisionId = revisionPageRef.getPage()->branchRevId.revisionId;
            return branchPage->lastRevisionPage;
        }
        for (RevisionPagePtr revisionPagePtr = branchPage->lastRevisionPage; revisionPagePtr; revisionPagePtr =
                revisionPageRef.getPage()->lastRevisionPage)
        {
            revisionPageRef = getPersistentStore().getAbsolutePage<RevisionPage>(revisionPagePtr);
            if (revisionPageRef.getPage()->branchRevId.revisionId == revisionId)
                return revisionPagePtr;
        }
        throwException(Exception, "Could not lookup revision %llx\n", revisionId);
        return NullPage;
    }

    RevisionId
    PersistentBranchManager::lookupRevision (BranchId branchId)
    {
        BranchPage* branchPage = getBranchPage(branchId);
        RevisionId revisionId = 0;
        RevisionPagePtr revisionPagePtr = lookupRevision(branchPage, revisionId);
        (void) revisionPagePtr;
#if PARANOID
        AbsolutePageRef<RevisionPage> revisionPageRef = getPersistentStore().getAbsolutePage<RevisionPage> ( revisionPagePtr );
        AssertBug ( revisionPageRef.getPage()->branchRevId.revisionId == revisionId, "Failed propagating revisionId !\n" );
#endif
        // return revisionPage->branchRevId.revisionId;
        return revisionId;
    }

    PersistentDocument*
    PersistentBranchManager::instanciatePersistentDocument (RevisionPagePtr revisionPagePtr, DocumentOpeningFlags flags,
                                                            PersistentDocumentAllocator* allocator)
    {
        AssertBug(revisionPagePtr, "RevisionPage was not found or not created.\n");

        if (!allocator)
        {
            allocator = new PersistentDocumentAllocator(getPersistentStore(), revisionPagePtr);
        }

        PersistentDocument* document = new PersistentDocument(getPersistentStore(), *allocator, flags);

        AssertBug(document, "Could not create document ???\n");
        if (!document)
        {
            Error("Could not create document !\n");
            return NULL;
        }

        Log_PBM ( "Created new document %p (%llx:%llx), allocator at %p, revPage = %llx\n",
                document, _brid(document->getBranchRevId()), allocator, revisionPagePtr );

        document->bindDocumentAllocator(allocator);

        return document;
    }

    PersistentDocument*
    PersistentBranchManager::instanciateTemporaryPersistentDocument (RevisionPagePtr revisionPagePtr)
    {
        PersistentDocument* doc = instanciatePersistentDocument(revisionPagePtr, DocumentOpeningFlags_UnManaged);
        return doc;
    }

    RevisionPagePtr
    PersistentBranchManager::lookupRevision (PersistentBranchInfo& branchInfo, RevisionId& revisionId,
                                             DocumentOpeningFlags flags)
    {
        assertIsLocked();
        /**
         * Perform the lookup.
         * If revisionId == 0, we will get the last revisionPage
         */
        RevisionPagePtr revisionPagePtr = lookupRevision(branchInfo.getBranchPage(), revisionId);
        // Log_PBM ( "Lookup revision %llx -> page %llx\n", revisionId, revisionPagePtr );

        if (!revisionPagePtr)
            return NullPage;

        /**
         * We have found a revisionPagePtr matching for this revisionId
         */
        AbsolutePageRef<RevisionPage> revisionPageRef = getPersistentStore().getAbsolutePage<RevisionPage>(revisionPagePtr);
        Log_PBM ( "After lookup, revision is [%llx:%llx] (writable=%llx)\n",
                _brid(revisionPageRef.getPage()->branchRevId), revisionPageRef.getPage()->documentAllocationHeader.writable );

        /**
         * Check if the document we got was writable
         */
        if (revisionPageRef.getPage()->documentAllocationHeader.writable)
        {
            /**
             * The revisionPage we have is writable
             */
            if (flags == DocumentOpeningFlags_Read || flags == DocumentOpeningFlags_ExplicitRead
                    || flags == DocumentOpeningFlags_Write)
            {
                Log_PBM ( "Revision is writable, but we wished not to have one.\n" );
                RevisionPagePtr lastRevisionPagePtr = revisionPageRef.getPage()->lastRevisionPage;

                if ( lastRevisionPagePtr )
                {
                    AbsolutePageRef<RevisionPage> lastRevisionPageRef = getPersistentStore().getAbsolutePage<RevisionPage> (lastRevisionPagePtr);
                    revisionId = lastRevisionPageRef.getPage()->branchRevId.revisionId;
                }
                else
                {
                    Log_PBM ( "Document flag was [Read|ExpicitRead|Write], but revision was already writable. Set to last revision (%llu).\n", lastRevisionPagePtr);
                }
                return lastRevisionPagePtr;
            }
            else if ( flags == DocumentOpeningFlags_ReuseWrite
                    || flags == DocumentOpeningFlags_AsRevision )
            {
                Log_PBM ( "Warn : revision is writable, but I will not lock this branch !\n" );
#if 0 // DEPRECATED
                Log_PBM ( "*** LOCK WRITE *** : Aquire lock for branch %llx !\n", branchInfo.getBranchId() );
                branchInfo.lockWrite ();

                if ( ! revisionPage->documentAllocationHeader.writable )
                {
                    Log_PBM ( "*** LOCK WRITE *** : revision is NO LONGER writable !\n" );
                    branchInfo.unlockWrite ();
                }
                Log_PBM ( "revision was writable, but that fits me well.\n" );
#endif
                return revisionPagePtr;
            }
            else if ( flags == DocumentOpeningFlags_FollowBranch )
            {
                Log_PBM ( "revision was writable, but that fits me well as i am following branch !\n" );
                return revisionPagePtr;
            }
            NotImplemented ( "Document was set writable, but flags=%s.\n", getDocumentOpeningFlagsName(flags) );
        }
        else if ( flags == DocumentOpeningFlags_Write )
        {
            Log_PBM ( "Revision was not set writable, but we *had* a write !\n" );
            return NullPage;
        }

        Log_PBM ( "Lookup revision %llx:%llx->%llx for flag %x gave page %llx\n",
                branchInfo.getBranchId(), revisionId, revisionPageRef.getPage()->branchRevId.revisionId, flags, revisionPagePtr );
        return revisionPagePtr;
    }

    PersistentDocument*
    PersistentBranchManager::instanciatePersistentDocument (BranchInfo& branchInfo, RevisionPagePtr revisionPagePtr,
                                                            DocumentOpeningFlags flags, RoleId roleId,
                                                            PersistentDocumentAllocator* allocator)
    {
        PersistentDocument* doc = instanciatePersistentDocument(revisionPagePtr, flags, allocator);
        AssertBug(doc, "Null document !\n");
        branchInfo.referenceDocument(doc);
        doc->setRoleId(roleId);
        Log_PBM ( "Effective openning of doc %p with flag=%x, role=%s at brId=%llx:%llx\n",
                doc, flags, getStore().getKeyCache().dumpKey(roleId).c_str(),
                _brid(doc->getBranchRevId()) );
        return doc;
    }

    Document*
    PersistentBranchManager::openDocument (BranchId branchId, RevisionId revisionId, DocumentOpeningFlags flags,
                                           RoleId roleId)
    {
        AssertBug(branchId, "Invalid zero branchId ! Branch lookup failed !\n");
        AssertBug(flags != DocumentOpeningFlags_UnManaged,
                  "Not a valid creation flag : %x (DocumentOpeningFlags_UnManaged)\n", flags);
        AssertBug(flags != DocumentOpeningFlags_ReuseWrite,
                  "Not a valid creation flag : %x (DocumentOpeningFlags_ReuseWrite)\n", flags);
        Log_PBM ( "Openning document branchId=%llx, revisionId=%llx, flags=%x (%s)\n", branchId, revisionId, flags,
                getDocumentOpeningFlagsName (flags) );

        Lock lock(branchManagerMutex);

        BranchInfo* genericBranchInfo = branchMap[branchId];
        if (!genericBranchInfo)
        {
            throwException(Exception, "Branch %llx does not exist !\n", branchId);
        }
        AssertBug(genericBranchInfo, "Branch %llx does not exist !\n", branchId);

        PersistentBranchInfo& branchInfo = dynamic_cast<PersistentBranchInfo&>(*genericBranchInfo);

        RevisionPagePtr revisionPagePtr = NullPage;

        if (flags == DocumentOpeningFlags_ResetRevision || flags == DocumentOpeningFlags_Write)
        {
            /*
             * Disable lookup for revision reset, and for revision _force_ write
             */
            branchInfo.lockWrite();
            bool noInherit = (flags == DocumentOpeningFlags_ResetRevision);
            revisionPagePtr = createRevision(branchInfo.getBranchPage(), noInherit);
            AssertBug(revisionPagePtr, "Could not create revision ??\n");
            Document* doc = instanciatePersistentDocument(branchInfo, revisionPagePtr, flags, roleId);
            return doc;
        }

        /**
         * Try to find if there is a revision that fits our needs
         */
        revisionPagePtr = lookupRevision(branchInfo, revisionId, flags);

        /**
         * We did not find any revision. Check if we can create one.
         */
        if (!revisionPagePtr)
        {
            Warn("Could not lookup revision %llx\n", revisionId);
            return NULL;
        }

        /**
         * We have to find if the document was in the cache or not.
         */
        if (1 || flags == DocumentOpeningFlags_Read || flags == DocumentOpeningFlags_ExplicitRead)
        {
            Document* doc = getDocumentFromCache(&branchInfo, revisionId, flags, roleId);
            if (doc)
            {
                return doc;
            }
        }

        /**
         * We have to check if we have a Document already openned here
         * WARN : THIS IS A NEW FEATURE, take with care.
         */
        if (flags == DocumentOpeningFlags_FollowBranch)
        {
            if (branchInfo.getInstanciatedDocumentsNumber())
            {
                if (branchInfo.getLastRevisionId() != revisionId)
                {
                    Log_PBM ( "At openDocument(branch=%llx,revision=%llx,flags=%x) : migration from revision %llx to revision %llx\n",
                            branchId, revisionId, flags, revisionId, branchInfo.getLastRevisionId() );
                    AssertBug ( branchInfo.isLockedWrite(), "Branch not locked write, wtf ?\n" );
                    revisionId = branchInfo.getLastRevisionId();
                }

                Document* sourceDocGeneric = branchInfo.getInstanciatedDocument(revisionId,DocumentOpeningFlags_FollowBranch);
                PersistentDocument* sourceDoc = dynamic_cast<PersistentDocument*> ( sourceDocGeneric );
                if ( sourceDoc )
                {
                    Log_PBM ( "[FOLLOWBRANCH] : Source Document at = %p : [%llx:%llx], refCount=%llu\n",
                            sourceDoc, _brid(sourceDoc->getBranchRevId()), sourceDoc->getRefCount() );

                    PersistentDocumentAllocator& allocator = sourceDoc->getPersistentDocumentAllocator();
                    Log_PBM ( "[FOLLOWBRANCH] Reuse documentAllocator at %p, refCount=%llu\n", &allocator, allocator.getRefCount() );

                    Document* doc = instanciatePersistentDocument
                    ( branchInfo, revisionPagePtr, flags, roleId, &allocator );
                    Log_PBM ( "[FOLLOWBRANCH] Instanciated document %p (from %p) for [%llx:%llx] using allocator %p\n", doc, sourceDoc,
                            _brid(doc->getBranchRevId() ), &allocator );
                    return doc;
                }
            }
        }

        PersistentDocument* doc = instanciatePersistentDocument(branchInfo, revisionPagePtr, flags, roleId);

        if (doc->getDocumentOpeningFlags() == DocumentOpeningFlags_ExplicitRead)
        {
            Log_PBM ( "**** Setting explicitRead [%llx:%llx] as reusable\n", _brid ( doc->getBranchRevId() ) );
            doc->lockRefCount();
            bool reusable = branchInfo.setReusableDocument ( doc );
            if ( ! reusable )
            {
                Log_PBM ( "*** Did not set reusable.\n" );
            }
            doc->unlockRefCount();
        }

        Log_PBM ( "Effective openning of persistentDocument brId=%llx:%llx\n", _brid(doc->getBranchRevId()) );
        return doc;
    }

    void
    PersistentBranchManager::checkBranchRemoval (BranchInfo* _branchInfo)
    {
        PersistentBranchInfo* branchInfo = dynamic_cast<PersistentBranchInfo*>(_branchInfo);
        AssertBug(branchInfo, "NULL branchInfo !\n");

        if ((branchInfo->isScheduledForRemoval() || branchInfo->getBranchPage()->lastRevisionPage == NullPage)
                && branchInfo->isFreeFromDocuments())
        {
            Log_PBM ( "At doc deref : Branch %llx to delete, scheduled=%s, has revisions=%s.\n",
                    branchInfo->getBranchId(),
                    branchInfo->isScheduledForRemoval() ? "yes" : "no",
                    branchInfo->getBranchPage()->lastRevisionPage == NullPage ? "yes" : "no" );

        deleteBranch ( branchInfo );
    }
}

}
;

