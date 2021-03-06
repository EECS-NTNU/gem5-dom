
/*
 * Copyright (c) 2010-2014, 2017-2020 ARM Limited
 * Copyright (c) 2013 Advanced Micro Devices, Inc.
 * All rights reserved
 *
 * The license below extends only to copyright in the software and shall
 * not be construed as granting a license to any other intellectual
 * property including but not limited to intellectual property relating
 * to a hardware implementation of the functionality of the software
 * licensed hereunder.  You may use the software subject to the license
 * terms below provided that you ensure that this notice is replicated
 * unmodified and in its entirety in all distributions of the software,
 * modified or unmodified, in source code or in binary form.
 *
 * Copyright (c) 2004-2005 The Regents of The University of Michigan
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __CPU_O3_LSQ_UNIT_IMPL_HH__
#define __CPU_O3_LSQ_UNIT_IMPL_HH__

#include "arch/generic/debugfaults.hh"
#include "arch/locked_mem.hh"
#include "base/str.hh"
#include "config/the_isa.hh"
#include "cpu/checker/cpu.hh"
#include "cpu/o3/lsq.hh"
#include "cpu/o3/lsq_unit.hh"
#include "debug/Activity.hh"
#include "debug/DebugDOM.hh"
#include "debug/HtmCpu.hh"
#include "debug/IEW.hh"
#include "debug/LSQUnit.hh"
#include "debug/O3PipeView.hh"
#include "mem/packet.hh"
#include "mem/request.hh"

template<class Impl>
LSQUnit<Impl>::WritebackEvent::WritebackEvent(const DynInstPtr &_inst,
        PacketPtr _pkt, LSQUnit *lsq_ptr)
    : Event(Default_Pri, AutoDelete),
      inst(_inst), pkt(_pkt), lsqPtr(lsq_ptr)
{
    assert(_inst->savedReq);
    _inst->savedReq->writebackScheduled();
}

template<class Impl>
void
LSQUnit<Impl>::WritebackEvent::process()
{
    assert(!lsqPtr->cpu->switchedOut());

    lsqPtr->writeback(inst, pkt);

    assert(inst->savedReq);
    inst->savedReq->writebackDone();
    delete pkt;
}

template<class Impl>
const char *
LSQUnit<Impl>::WritebackEvent::description() const
{
    return "Store writeback";
}

template <class Impl>
bool
LSQUnit<Impl>::recvTimingResp(PacketPtr pkt)
{
    auto senderState = dynamic_cast<LSQSenderState*>(pkt->senderState);
    LSQRequest* req = senderState->request();
    assert(req != nullptr);
    DPRINTF(DebugDOM, "Received timing resp for pkt %s, with spec %d"
            ", predictable: %d, domMode: %d, mpspemMode: %d\n",
            pkt->print(), pkt->isSpeculative(),
            pkt->isPredictable(), pkt->isDomMode(),
            pkt->isMpspemMode());
    bool ret = true;
    /* Check that the request is still alive before any further action. */
    if (senderState->alive()) {
        ret = req->recvTimingResp(pkt);
        if (pkt->isPredictedAddress) {
            req->discard();
        }
    } else {
        senderState->outstanding--;
    }
    return ret;

}

template<class Impl>
void
LSQUnit<Impl>::completeDataAccess(PacketPtr pkt)
{
    LSQSenderState *state = dynamic_cast<LSQSenderState *>(pkt->senderState);
    DynInstPtr inst = state->inst;

    DPRINTF(DebugDOM, "Data access completion for [sn:%llu], with "
            "cShadow: %d, dShadow: %d, pktSpec: %d, specmode: %d\n",
            inst->seqNum,
            inst->cShadow,
            inst->dShadow,
            pkt->speculative,
            pkt->speculativeMode);

    // hardware transactional memory
    // sanity check
    if (pkt->isHtmTransactional() && !inst->isSquashed()) {
        assert(inst->getHtmTransactionUid() == pkt->getHtmTransactionUid());
    }

    // if in a HTM transaction, it's possible
    // to abort within the cache hierarchy.
    // This is signalled back to the processor
    // through responses to memory requests.
    if (pkt->htmTransactionFailedInCache()) {
        // cannot do this for write requests because
        // they cannot tolerate faults
        const HtmCacheFailure htm_rc =
            pkt->getHtmTransactionFailedInCacheRC();
        if (pkt->isWrite()) {
            DPRINTF(HtmCpu,
                "store notification (ignored) of HTM transaction failure "
                "in cache - addr=0x%lx - rc=%s - htmUid=%d\n",
                pkt->getAddr(), htmFailureToStr(htm_rc),
                pkt->getHtmTransactionUid());
        } else {
            HtmFailureFaultCause fail_reason =
                HtmFailureFaultCause::INVALID;

            if (htm_rc == HtmCacheFailure::FAIL_SELF) {
                fail_reason = HtmFailureFaultCause::SIZE;
            } else if (htm_rc == HtmCacheFailure::FAIL_REMOTE) {
                fail_reason = HtmFailureFaultCause::MEMORY;
            } else if (htm_rc == HtmCacheFailure::FAIL_OTHER) {
                // these are likely loads that were issued out of order
                // they are faulted here, but it's unlikely that these will
                // ever reach the commit head.
                fail_reason = HtmFailureFaultCause::OTHER;
            } else {
                panic("HTM error - unhandled return code from cache (%s)",
                      htmFailureToStr(htm_rc));
            }

            inst->fault =
            std::make_shared<GenericHtmFailureFault>(
                inst->getHtmTransactionUid(),
                fail_reason);

            DPRINTF(HtmCpu,
                "load notification of HTM transaction failure "
                "in cache - pc=%s - addr=0x%lx - "
                "rc=%u - htmUid=%d\n",
                inst->pcState(), pkt->getAddr(),
                htmFailureToStr(htm_rc), pkt->getHtmTransactionUid());
        }
    }

    cpu->ppDataAccessComplete->notify(std::make_pair(inst, pkt));

    /* Notify the sender state that the access is complete (for ownership
     * tracking). */
    state->complete();

    assert(!cpu->switchedOut());
    if (!inst->isSquashed()) {
        if (cpu->MP &&
            state->needWB && (inst->underShadow() && inst->isLoad()) &&
            (!pkt->isPredictable())) {
            DPRINTF(DOM, "Saved complete response for [sn:%llu]\n",
                    inst->seqNum);
            assert(!inst->shouldForward);
            inst->storeResp(pkt);
        } else if (state->needWB) {
            // Only loads, store conditionals and atomics perform the writeback
            // after receving the response from the memory
            assert(inst->isLoad() || inst->isStoreConditional() ||
                   inst->isAtomic());

            // hardware transactional memory
            if (pkt->htmTransactionFailedInCache()) {
                state->request()->mainPacket()->setHtmTransactionFailedInCache(
                    pkt->getHtmTransactionFailedInCacheRC() );
            }

            writeback(inst, state->request()->mainPacket());
            if (inst->isStore() || inst->isAtomic()) {
                auto ss = dynamic_cast<SQSenderState*>(state);
                ss->writebackDone();
                completeStore(ss->idx);
            }
        } else if (inst->isStore()) {
            // This is a regular store (i.e., not store conditionals and
            // atomics), so it can complete without writing back
            completeStore(dynamic_cast<SQSenderState*>(state)->idx);
        }
    }
}

template <class Impl>
LSQUnit<Impl>::LSQUnit(uint32_t lqEntries, uint32_t sqEntries)
    : lsqID(-1), storeQueue(sqEntries+1), loadQueue(lqEntries+1),
      loads(0), stores(0), storesToWB(0),
      htmStarts(0), htmStops(0),
      lastRetiredHtmUid(0),
      cacheBlockMask(0), stalled(false),
      isStoreBlocked(false), storeInFlight(false), hasPendingRequest(false),
      pendingRequest(nullptr), stats(nullptr)
{
    srand(10111997);
}

template<class Impl>
void
LSQUnit<Impl>::init(O3CPU *cpu_ptr, IEW *iew_ptr,
        const DerivO3CPUParams &params, LSQ *lsq_ptr, unsigned id,
        SimplePred<Impl> *pred)
{
    lsqID = id;

    cpu = cpu_ptr;
    iewStage = iew_ptr;

    lsq = lsq_ptr;

    cpu->addStatGroup(csprintf("lsq%i", lsqID).c_str(), &stats);

    DPRINTF(LSQUnit, "Creating LSQUnit%i object.\n",lsqID);

    depCheckShift = params.LSQDepCheckShift;
    checkLoads = params.LSQCheckLoads;
    needsTSO = params.needsTSO;

    add_pred = pred;

    resetState();
}


template<class Impl>
void
LSQUnit<Impl>::resetState()
{
    loads = stores = storesToWB = 0;

    // hardware transactional memory
    // nesting depth
    htmStarts = htmStops = 0;

    storeWBIt = storeQueue.begin();

    retryPkt = NULL;
    memDepViolator = NULL;

    stalled = false;

    cacheBlockMask = ~(cpu->cacheLineSize() - 1);
}

template<class Impl>
std::string
LSQUnit<Impl>::name() const
{
    if (Impl::MaxThreads == 1) {
        return iewStage->name() + ".lsq";
    } else {
        return iewStage->name() + ".lsq.thread" + std::to_string(lsqID);
    }
}

template <class Impl>
LSQUnit<Impl>::LSQUnitStats::LSQUnitStats(Stats::Group *parent)
    : Stats::Group(parent),
      ADD_STAT(forwLoads, UNIT_COUNT,
               "Number of loads that had data forwarded from stores"),
      ADD_STAT(squashedLoads, UNIT_COUNT,
               "Number of loads squashed"),
      ADD_STAT(ignoredResponses, UNIT_COUNT,
               "Number of memory responses ignored because the instruction is "
               "squashed"),
      ADD_STAT(memOrderViolation, UNIT_COUNT,
               "Number of memory ordering violations"),
      ADD_STAT(squashedStores, UNIT_COUNT, "Number of stores squashed"),
      ADD_STAT(rescheduledLoads, UNIT_COUNT,
               "Number of loads that were rescheduled"),
      ADD_STAT(blockedByCache, UNIT_COUNT,
               "Number of times an access to memory failed due to the cache "
               "being blocked"),
      ADD_STAT(loadsDelayedOnMiss, UNIT_COUNT,
               "Number of loads delayed because of missing in L1 Cache (DoM)"),
      ADD_STAT(issuedSnoops, UNIT_COUNT,
               "Number of snoops issued to check if load in L1 Cache"),
      ADD_STAT(valuePredictedLoads, UNIT_COUNT,
               "Number of L1 loads issued where value will be predicted"),
      ADD_STAT(failedValuePredictions, UNIT_COUNT,
               "Number of L1 loads where value could not be predicted"),
      ADD_STAT(preloadedLoads, UNIT_COUNT,
               "Number of L1 misses which are issued for preloading"),
      ADD_STAT(normalIssuedLoads, UNIT_COUNT,
               "Number of loads that are issued normally (no speculation"),
      ADD_STAT(loadsIssuedOnHit, UNIT_COUNT,
               "Number of L1 hits issued (DoM)"),
      ADD_STAT(earlyIssues, UNIT_COUNT,
               "Loads issued earlier due to address prediction"),
      ADD_STAT(extraIssues, UNIT_COUNT,
               "Extra loads issued due to faulty address predictions"),
      ADD_STAT(blockedPredictedPreloads, UNIT_COUNT,
               "Predicted address loads not able to issue"),
      ADD_STAT(predictedPreloads, UNIT_COUNT,
               "Predicted address loads that preload"),
      ADD_STAT(predictedHits, UNIT_COUNT,
               "Predicted address loads that hit in L1 cache"),
      ADD_STAT(addrPredictions, UNIT_COUNT,
               "Total number of address predictions made"),
      ADD_STAT(emptyAddrPredictions, UNIT_COUNT,
               "Address predictions that are empty (unknown pattern)"),
      ADD_STAT(nonAddrPredictedLoads, UNIT_COUNT,
               "Loads that were not address predicted at all"),
      ADD_STAT(correctlyAddressPredictedLoads, UNIT_COUNT,
               "Loads that were correctly address predicted (commit)"),
      ADD_STAT(wronglyAddressPredictedLoads, UNIT_COUNT,
               "Loads that were incorrectly address predicted (commit)"),
      ADD_STAT(splitAddressPredictionsDropped, UNIT_COUNT,
               "Predictions dropped due to being split"),
      ADD_STAT(failedTranslationsFromPredictions, UNIT_COUNT,
               "Addr predictions that failed translation"),
      ADD_STAT(issuedAddressPredictions, UNIT_COUNT,
               "Addr predicted loads issued to memory"),
      ADD_STAT(failedToIssuePredictions, UNIT_COUNT,
               "Final stage predictions that got rejected by cache port"),
      ADD_STAT(forwardedStoreData, UNIT_COUNT,
               "Addr predicted loads issued to memory"),
      ADD_STAT(forwardedPredictedData, UNIT_COUNT,
               "Addr predicted loads issued to memory"),
      ADD_STAT(forwardedToPredictions, UNIT_COUNT,
               "Addr predicted loads issued to memory"),
      ADD_STAT(wrongSizeRightAddress, UNIT_COUNT,
               "Addr predictions with right address and wrong size"),
      ADD_STAT(partialPredStoreConflicts, UNIT_COUNT,
               "Addr predictions with partially conflicting stores"),
      ADD_STAT(conflictDroppedPreds, UNIT_COUNT,
               "Correct addr predictions dropped due to conflicting stores"),
      ADD_STAT(incorrectPredData, UNIT_COUNT,
               "Verify accesses with different data than pred"),
      ADD_STAT(incorrectStoredData, UNIT_COUNT,
               "Verify accesses with different data than stored"),
      ADD_STAT(cShadowClearedFirst, UNIT_COUNT,
               "Insts which for C shadow was cleared first"),
      ADD_STAT(dShadowClearedFirst, UNIT_COUNT,
               "Insts which for D shadow was cleared first"),
      ADD_STAT(predResolutionTime, UNIT_TICK,
               "Resolution (physical) time for predicted addresses")
{
    predResolutionTime
        .init(0, 98, 3)
        .flags(Stats::total);
}

template<class Impl>
void
LSQUnit<Impl>::setDcachePort(RequestPort *dcache_port)
{
    dcachePort = dcache_port;
}

template<class Impl>
void
LSQUnit<Impl>::drainSanityCheck() const
{
    for (int i = 0; i < loadQueue.capacity(); ++i)
        assert(!loadQueue[i].valid());

    assert(storesToWB == 0);
    assert(!retryPkt);
}

template<class Impl>
void
LSQUnit<Impl>::takeOverFrom()
{
    resetState();
}

template <class Impl>
void
LSQUnit<Impl>::insert(const DynInstPtr &inst)
{
    assert(inst->isMemRef());

    assert(inst->isLoad() || inst->isStore() || inst->isAtomic());

    if (inst->isLoad()) {
        insertLoad(inst);
    } else {
        insertStore(inst);
    }

    inst->setInLSQ();
}

template <class Impl>
void
LSQUnit<Impl>::insertLoad(const DynInstPtr &load_inst)
{
    assert(!loadQueue.full());
    assert(loads < loadQueue.capacity());

    DPRINTF(LSQUnit, "Inserting load PC %s, idx:%i [sn:%lli]\n",
            load_inst->pcState(), loadQueue.tail(), load_inst->seqNum);

    /* Grow the queue. */
    loadQueue.advance_tail();

    load_inst->sqIt = storeQueue.end();

    assert(!loadQueue.back().valid());
    loadQueue.back().set(load_inst);
    load_inst->lqIdx = loadQueue.tail();
    assert(load_inst->lqIdx > 0);
    load_inst->lqIt = loadQueue.getIterator(load_inst->lqIdx);

    ++loads;

    updateDShadow(load_inst->lqIt->instPtr());
    iewStage->instQueue.propagateTaints(load_inst,
                                        load_inst->threadNumber);

    // hardware transactional memory
    // transactional state and nesting depth must be tracked
    // in the in-order part of the core.
    if (load_inst->isHtmStart()) {
        htmStarts++;
        DPRINTF(HtmCpu, ">> htmStarts++ (%d) : htmStops (%d)\n",
                htmStarts, htmStops);

        const int htm_depth = htmStarts - htmStops;
        const auto& htm_cpt = cpu->tcBase(lsqID)->getHtmCheckpointPtr();
        auto htm_uid = htm_cpt->getHtmUid();

        // for debugging purposes
        if (!load_inst->inHtmTransactionalState()) {
            htm_uid = htm_cpt->newHtmUid();
            DPRINTF(HtmCpu, "generating new htmUid=%u\n", htm_uid);
            if (htm_depth != 1) {
                DPRINTF(HtmCpu,
                    "unusual HTM transactional depth (%d)"
                    " possibly caused by mispeculation - htmUid=%u\n",
                    htm_depth, htm_uid);
            }
        }
        load_inst->setHtmTransactionalState(htm_uid, htm_depth);
    }

    if (load_inst->isHtmStop()) {
        htmStops++;
        DPRINTF(HtmCpu, ">> htmStarts (%d) : htmStops++ (%d)\n",
                htmStarts, htmStops);

        if (htmStops==1 && htmStarts==0) {
            DPRINTF(HtmCpu,
            "htmStops==1 && htmStarts==0. "
            "This generally shouldn't happen "
            "(unless due to misspeculation)\n");
        }
    }
}

template <class Impl>
void
LSQUnit<Impl>::insertStore(const DynInstPtr& store_inst)
{
    // Make sure it is not full before inserting an instruction.
    assert(!storeQueue.full());
    assert(stores < storeQueue.capacity());

    DPRINTF(LSQUnit, "Inserting store PC %s, idx:%i [sn:%lli]\n",
            store_inst->pcState(), storeQueue.tail(), store_inst->seqNum);
    storeQueue.advance_tail();

    store_inst->sqIdx = storeQueue.tail();
    store_inst->lqIdx = loadQueue.tail() + 1;
    assert(store_inst->lqIdx > 0);
    store_inst->lqIt = loadQueue.end();

    storeQueue.back().set(store_inst);

    ++stores;
}

template <class Impl>
typename Impl::DynInstPtr
LSQUnit<Impl>::getMemDepViolator()
{
    DynInstPtr temp = memDepViolator;

    memDepViolator = NULL;

    return temp;
}

template <class Impl>
unsigned
LSQUnit<Impl>::numFreeLoadEntries()
{
        //LQ has an extra dummy entry to differentiate
        //empty/full conditions. Subtract 1 from the free entries.
        DPRINTF(LSQUnit, "LQ size: %d, #loads occupied: %d\n",
                1 + loadQueue.capacity(), loads);
        return loadQueue.capacity() - loads;
}

template <class Impl>
unsigned
LSQUnit<Impl>::numFreeStoreEntries()
{
        //SQ has an extra dummy entry to differentiate
        //empty/full conditions. Subtract 1 from the free entries.
        DPRINTF(LSQUnit, "SQ size: %d, #stores occupied: %d\n",
                1 + storeQueue.capacity(), stores);
        return storeQueue.capacity() - stores;

 }

template <class Impl>
void
LSQUnit<Impl>::checkSnoop(PacketPtr pkt)
{
    // Should only ever get invalidations in here
    assert(pkt->isInvalidate());

    DPRINTF(LSQUnit, "Got snoop for address %#x\n", pkt->getAddr());

    for (int x = 0; x < cpu->numContexts(); x++) {
        ThreadContext *tc = cpu->getContext(x);
        bool no_squash = cpu->thread[x]->noSquashFromTC;
        cpu->thread[x]->noSquashFromTC = true;
        TheISA::handleLockedSnoop(tc, pkt, cacheBlockMask);
        cpu->thread[x]->noSquashFromTC = no_squash;
    }

    if (loadQueue.empty())
        return;

    auto iter = loadQueue.begin();

    Addr invalidate_addr = pkt->getAddr() & cacheBlockMask;

    DynInstPtr ld_inst = iter->instruction();
    assert(ld_inst);
    LSQRequest *req = iter->request();

    // Check that this snoop didn't just invalidate our lock flag
    if (ld_inst->effAddrValid() &&
        req->isCacheBlockHit(invalidate_addr, cacheBlockMask)
        && ld_inst->memReqFlags & Request::LLSC)
        TheISA::handleLockedSnoopHit(ld_inst.get());

    bool force_squash = false;

    while (++iter != loadQueue.end()) {
        ld_inst = iter->instruction();
        assert(ld_inst);
        req = iter->request();
        if (!ld_inst->effAddrValid() || ld_inst->strictlyOrdered())
            continue;

        DPRINTF(LSQUnit, "-- inst [sn:%lli] to pktAddr:%#x\n",
                    ld_inst->seqNum, invalidate_addr);

        if (force_squash ||
            req->isCacheBlockHit(invalidate_addr, cacheBlockMask)) {
            if (needsTSO) {
                // If we have a TSO system, as all loads must be ordered with
                // all other loads, this load as well as *all* subsequent loads
                // need to be squashed to prevent possible load reordering.
                force_squash = true;
            }
            if (ld_inst->possibleLoadViolation() || force_squash) {
                DPRINTF(LSQUnit, "Conflicting load at addr %#x [sn:%lli]\n",
                        pkt->getAddr(), ld_inst->seqNum);

                // Mark the load for re-execution
                ld_inst->fault = std::make_shared<ReExec>();
                req->setStateToFault();
            } else {
                DPRINTF(LSQUnit, "HitExternal Snoop for addr %#x [sn:%lli]\n",
                        pkt->getAddr(), ld_inst->seqNum);

                // Make sure that we don't lose a snoop hitting a LOCKED
                // address since the LOCK* flags don't get updated until
                // commit.
                if (ld_inst->memReqFlags & Request::LLSC)
                    TheISA::handleLockedSnoopHit(ld_inst.get());

                // If a older load checks this and it's true
                // then we might have missed the snoop
                // in which case we need to invalidate to be sure
                ld_inst->hitExternalSnoop(true);
            }
        }
    }
    return;
}

template <class Impl>
Fault
LSQUnit<Impl>::checkViolations(typename LoadQueue::iterator& loadIt,
        const DynInstPtr& inst)
{
    Addr inst_eff_addr1 = inst->effAddr >> depCheckShift;
    Addr inst_eff_addr2 = (inst->effAddr + inst->effSize - 1) >> depCheckShift;

    /** @todo in theory you only need to check an instruction that has executed
     * however, there isn't a good way in the pipeline at the moment to check
     * all instructions that will execute before the store writes back. Thus,
     * like the implementation that came before it, we're overly conservative.
     */
    while (loadIt != loadQueue.end()) {
        DynInstPtr ld_inst = loadIt->instruction();
        if (!ld_inst->effAddrValid() || ld_inst->strictlyOrdered()) {
            ++loadIt;
            continue;
        }

        Addr ld_eff_addr1 = ld_inst->effAddr >> depCheckShift;
        Addr ld_eff_addr2 =
            (ld_inst->effAddr + ld_inst->effSize - 1) >> depCheckShift;

        if (inst_eff_addr2 >= ld_eff_addr1 && inst_eff_addr1 <= ld_eff_addr2) {
            if (inst->isLoad()) {
                // If this load is to the same block as an external snoop
                // invalidate that we've observed then the load needs to be
                // squashed as it could have newer data
                if (ld_inst->hitExternalSnoop()) {
                    if (!memDepViolator ||
                            ld_inst->seqNum < memDepViolator->seqNum) {
                        DPRINTF(LSQUnit, "Detected fault with inst [sn:%lli] "
                                "and [sn:%lli] at address %#x\n",
                                inst->seqNum, ld_inst->seqNum, ld_eff_addr1);
                        memDepViolator = ld_inst;

                        ++stats.memOrderViolation;

                        return std::make_shared<GenericISA::M5PanicFault>(
                            "Detected fault with inst [sn:%lli] and "
                            "[sn:%lli] at address %#x\n",
                            inst->seqNum, ld_inst->seqNum, ld_eff_addr1);
                    }
                }

                // Otherwise, mark the load has a possible load violation and
                // if we see a snoop before it's commited, we need to squash
                ld_inst->possibleLoadViolation(true);
                DPRINTF(LSQUnit, "Found possible load violation at addr: %#x"
                        " between instructions [sn:%lli] and [sn:%lli]\n",
                        inst_eff_addr1, inst->seqNum, ld_inst->seqNum);
            } else {
                // A load/store incorrectly passed this store.
                // Check if we already have a violator, or if it's newer
                // squash and refetch.
                if (memDepViolator && ld_inst->seqNum > memDepViolator->seqNum)
                    break;

                DPRINTF(LSQUnit, "Detected fault with inst [sn:%lli] and "
                        "[sn:%lli] at address %#x\n",
                        inst->seqNum, ld_inst->seqNum, ld_eff_addr1);
                memDepViolator = ld_inst;

                ++stats.memOrderViolation;

                return std::make_shared<GenericISA::M5PanicFault>(
                    "Detected fault with "
                    "inst [sn:%lli] and [sn:%lli] at address %#x\n",
                    inst->seqNum, ld_inst->seqNum, ld_eff_addr1);
            }
        }

        ++loadIt;
    }
    return NoFault;
}




template <class Impl>
Fault
LSQUnit<Impl>::executeLoad(const DynInstPtr &inst)
{
    // Execute a specific load.
    Fault load_fault = NoFault;

    DPRINTF(LSQUnit, "Executing load PC %s, [sn:%lli]\n",
            inst->pcState(), inst->seqNum);

    assert(!inst->isSquashed());

    load_fault = inst->initiateAcc();
    DPRINTF(DebugDOM, "Finished load execution,"
    "fault is : %s\n", load_fault != NoFault ? load_fault->name() :
    "none");

    if (load_fault == NoFault && !inst->readMemAccPredicate()
        && !inst->shouldForward) {
        assert(inst->readPredicate());
        inst->setExecuted();
        inst->completeAcc(nullptr);
        iewStage->instToCommit(inst);
        iewStage->activityThisCycle();
        return NoFault;
    }

    if (load_fault == NoFault && inst->shouldForward) {
        iewStage->activityThisCycle();
        return NoFault;
    }

    if (inst->isTranslationDelayed() && load_fault == NoFault)
        return load_fault;

    if (load_fault != NoFault && inst->translationCompleted() && inst->savedReq
        && inst->savedReq->isPartialFault() && !inst->savedReq->isComplete()) {
        assert(inst->savedReq->isSplit());
        // If we have a partial fault where the mem access is not complete yet
        // then the cache must have been blocked. This load will be re-executed
        // when the cache gets unblocked. We will handle the fault when the
        // mem access is complete.
        return NoFault;
    }
    // If the instruction faulted and is under shadow, reschedule it
    if (load_fault != NoFault && load_fault->isShadow()) {
        iewStage->activityThisCycle();
        DPRINTF(DebugDOM, "Caught Fault: %s. Not committing\n",
            load_fault->name());
        return NoFault;
    }
    // If the instruction faulted or predicated false, then we need to send it
    // along to commit without the instruction completing.
    else if (load_fault != NoFault || !inst->readPredicate()) {
        // Send this instruction to commit, also make sure iew stage
        // realizes there is activity.  Mark it as executed unless it
        // is a strictly ordered load that needs to hit the head of
        // commit.
        if (!inst->readPredicate())
            inst->forwardOldRegs();
        DPRINTF(LSQUnit, "Load [sn:%lli] not executed from %s\n",
                inst->seqNum,
                (load_fault != NoFault ? "fault" : "predication"));
        if (!(inst->hasRequest() && inst->strictlyOrdered()) ||
            inst->isAtCommit()) {
            inst->setExecuted();
        }
        iewStage->instToCommit(inst);
        iewStage->activityThisCycle();
    } else {
        if (inst->effAddrValid()) {
            auto it = inst->lqIt;
            ++it;

            if (checkLoads)
                return checkViolations(it, inst);
        }
    }

    return load_fault;
}

template <class Impl>
Fault
LSQUnit<Impl>::executeStore(const DynInstPtr &store_inst)
{
    // Make sure that a store exists.
    assert(stores != 0);

    int store_idx = store_inst->sqIdx;

    DPRINTF(LSQUnit, "Executing store PC %s [sn:%lli]\n",
            store_inst->pcState(), store_inst->seqNum);

    assert(!store_inst->isSquashed());

    // Check the recently completed loads to see if any match this store's
    // address.  If so, then we have a memory ordering violation.
    typename LoadQueue::iterator loadIt = store_inst->lqIt;

    Fault store_fault = store_inst->initiateAcc();

    if (store_inst->isTranslationDelayed() &&
        store_fault == NoFault)
        return store_fault;

    if (!store_inst->readPredicate()) {
        DPRINTF(LSQUnit, "Store [sn:%lli] not executed from predication\n",
                store_inst->seqNum);
        store_inst->forwardOldRegs();
        return store_fault;
    }

    if (storeQueue[store_idx].size() == 0) {
        DPRINTF(LSQUnit,"Fault on Store PC %s, [sn:%lli], Size = 0\n",
                store_inst->pcState(), store_inst->seqNum);

        return store_fault;
    }

    assert(store_fault == NoFault);

    if (store_inst->isStoreConditional() || store_inst->isAtomic()) {
        // Store conditionals and Atomics need to set themselves as able to
        // writeback if we haven't had a fault by here.
        storeQueue[store_idx].canWB() = true;

        ++storesToWB;
    }

    return checkViolations(loadIt, store_inst);

}

template<class Impl>
void
LSQUnit<Impl>::updateDShadow(DynInstPtr &load_inst)
{
    DPRINTF(DebugDOM, "Updating D Shadow for [sn:%llu]\n",
            load_inst->seqNum);
    auto store_it = load_inst->sqIt;
    assert (store_it >= storeWBIt);
    while (store_it != storeWBIt) {
        store_it--;
        assert(store_it->valid());
        assert(store_it->instruction()->seqNum < load_inst->seqNum);
        if (!store_it->instruction()->effAddrValid()) {
            load_inst->dShadow = true;
            return;
        }
    }
    DPRINTF(DOM, "Cleared dShadow for [sn:%llu]\n",
            load_inst->seqNum);
    if (load_inst->dShadow) {
        if (load_inst->cShadow) {
            ++stats.dShadowClearedFirst;
        } else {
            ++stats.cShadowClearedFirst;
        }
    }
    load_inst->dShadow = false;
}

template<class Impl>
void
LSQUnit<Impl>::walkDShadows(const DynInstPtr &store_inst)
{
    if (!(cpu->safeMode)) return;
    auto load_it = store_inst->lqIt;
    DPRINTF(DebugDOM, "Walking younger loads for [sn:%llu]\n",
            store_inst->seqNum);
    while (load_it != loadQueue.end()) {
        updateDShadow(load_it->instPtr());
        DPRINTF(DebugDOM, "D Shadow for [sn:%llu] now %d\n",
                load_it->instPtr()->seqNum,
                load_it->instPtr()->dShadow);
        load_it++;
    }
}

template <class Impl>
void
LSQUnit<Impl>::predictLoad(DynInstPtr &inst)
{
    assert(!inst->isRanAhead());
    updateRunAhead(inst->instAddr(), 1);
    inst->setRanAhead(true);
    // Get a predicted virtual address
    Addr prediction = add_pred->predictFromPC(inst->instAddr(),
        runAhead[inst->instAddr()]);

    DPRINTF(AddrPrediction, "Making prediction on inst [sn:%llu]"
            " for PC [%llx] with vaddr %#x and runahead: %d\n",
            inst->seqNum,
            inst->instAddr(),
            prediction,
            runAhead[inst->instAddr()]);
    ++stats.addrPredictions;

    if (prediction == 0) {
        DPRINTF(AddrPrediction, "Tried to predict on load"
        " with no history, returning\n");

        ++stats.emptyAddrPredictions;
        return;
    }

    int packetSize = add_pred->getPacketSize(inst->instAddr());
    if (!inst->predData){
        inst->predData = new uint8_t[packetSize];
        memset(inst->predData, 0, packetSize);

    }

    // Try to translate address. Drop it if deferred
    Request::Flags _flags = 0x0000;
    LSQRequest *req = new PredictDataRequest(this, inst, prediction,
                                            packetSize, _flags);

    bool needs_burst = transferNeedsBurst(prediction, packetSize, 64);

    if (needs_burst) {
        DPRINTF(AddrPrediction, "Dropping prediction as it is split\n");
        assert(!req->isAnyOutstandingRequest());
        ++stats.splitAddressPredictionsDropped;
        req->discard();
        return;
    }
    req->initiateTranslation();

    if (!req->isMemAccessRequired()) {
        DPRINTF(AddrPrediction, "Addr prediction faulted\n");
        assert(!req->isAnyOutstandingRequest());
        req->discard();
        ++stats.failedTranslationsFromPredictions;
        return;
    }
    req->buildPackets();

    LSQSenderState *state = new LQSnoopState(req);
    state->inst = inst;

    req->senderState(state);

    req->sendPacketToCache();

    if (req->isSent()) {
        inst->setPredAddr(prediction, packetSize);
        inst->recvPredTick = curTick();
        addToPredInsts(inst);
        ++stats.issuedAddressPredictions;
        debugPredLoad(inst, req);
    } else {
        assert(!req->isAnyOutstandingRequest());
        req->discard();
        ++stats.failedToIssuePredictions;
    }
}

template <class Impl>
void
LSQUnit<Impl>::debugPredLoad(const DynInstPtr &load_inst, LSQRequest *req)
{
    auto request = std::make_shared<Request>(
        req->mainRequest()->getPaddr(), load_inst->getPredSize(),
        0x0000, load_inst->requestorId(),
        load_inst->instAddr(), load_inst->contextId(),
        nullptr
    );
    request->setPaddr(req->mainRequest()->getPaddr());
    assert(request->hasPaddr());
    PacketPtr snoop = Packet::createRead(request);
    if (!load_inst->verifyData){
        load_inst->verifyData = new uint8_t[load_inst->getPredSize()];
        memset(load_inst->verifyData, 0, load_inst->getPredSize());
    }
    snoop->dataStatic(load_inst->verifyData);
    snoop->verificationSpeculativeMode();
    LSQSenderState *state = new LQSnoopState(req);
    snoop->senderState = state;
    dcachePort->sendFunctional(snoop);
    delete(snoop);
    delete(state);

    DPRINTF(AddrPredDebug, "Issuing Predicted Address load "
            "instant cache response:\n"
            "%#x:%#x:%#x:%#x:%#x:%#x:%#x:%#x\n",
            load_inst->verifyData[0],
            load_inst->verifyData[1],
            load_inst->verifyData[2],
            load_inst->verifyData[3],
            load_inst->verifyData[4],
            load_inst->verifyData[5],
            load_inst->verifyData[6],
            load_inst->verifyData[7]);
}

template <class Impl>
void
LSQUnit<Impl>::updatePredictor(const DynInstPtr &inst)
{
    add_pred->updatePredictor(inst->effAddr,
                              inst->instAddr(),
                              inst->seqNum,
                              inst->effSize);
    if (inst->predAddr == 0) {
        ++stats.nonAddrPredictedLoads;
    } else if (inst->predAddr == inst->effAddr) {
        ++stats.correctlyAddressPredictedLoads;
    } else {
        ++stats.wronglyAddressPredictedLoads;
    }
    DPRINTF(AddrPrediction, "Updating predictor with [sn:%llu],"
            "PC [%llx], pred_addr %#x, real_addr: %#x\n",
            inst->seqNum,
            inst->instAddr(),
            inst->predAddr,
            inst->effAddr);
}

template <class Impl>
void
LSQUnit<Impl>::commitLoad()
{
    assert(loadQueue.front().valid());

    DPRINTF(LSQUnit, "Committing head load instruction, PC %s\n",
            loadQueue.front().instruction()->pcState());

    auto inst = loadQueue.front().instruction();

    if (cpu->AP) {
        updatePredictor(inst);
        updateRunAhead(inst->instAddr(), -1);
    }

    loadQueue.front().clear();
    loadQueue.pop_front();

    --loads;
}

template <class Impl>
void
LSQUnit<Impl>::commitLoads(InstSeqNum &youngest_inst)
{
    assert(loads == 0 || loadQueue.front().valid());

    while (loads != 0 && loadQueue.front().instruction()->seqNum
            <= youngest_inst) {
        commitLoad();
    }
}

template <class Impl>
void
LSQUnit<Impl>::commitStores(InstSeqNum &youngest_inst)
{
    assert(stores == 0 || storeQueue.front().valid());

    /* Forward iterate the store queue (age order). */
    for (auto& x : storeQueue) {
        assert(x.valid());
        // Mark any stores that are now committed and have not yet
        // been marked as able to write back.
        if (!x.canWB()) {
            if (x.instruction()->seqNum > youngest_inst) {
                break;
            }
            DPRINTF(LSQUnit, "Marking store as able to write back, PC "
                    "%s [sn:%lli]\n",
                    x.instruction()->pcState(),
                    x.instruction()->seqNum);

            x.canWB() = true;

            ++storesToWB;
        }
    }
}

template <class Impl>
void
LSQUnit<Impl>::writebackBlockedStore()
{
    assert(isStoreBlocked);
    storeWBIt->request()->sendPacketToCache();
    if (storeWBIt->request()->isSent()){
        storePostSend();
    }
}

template <class Impl>
void
LSQUnit<Impl>::writebackStores()
{
    if (isStoreBlocked) {
        DPRINTF(LSQUnit, "Writing back  blocked store\n");
        writebackBlockedStore();
    }

    while (storesToWB > 0 &&
           storeWBIt.dereferenceable() &&
           storeWBIt->valid() &&
           storeWBIt->canWB() &&
           ((!needsTSO) || (!storeInFlight)) &&
           lsq->cachePortAvailable(false)) {

        if (isStoreBlocked) {
            DPRINTF(LSQUnit, "Unable to write back any more stores, cache"
                    " is blocked!\n");
            break;
        }

        // Store didn't write any data so no need to write it back to
        // memory.
        if (storeWBIt->size() == 0) {
            /* It is important that the preincrement happens at (or before)
             * the call, as the the code of completeStore checks
             * storeWBIt. */
            completeStore(storeWBIt++);
            continue;
        }

        if (storeWBIt->instruction()->isDataPrefetch()) {
            storeWBIt++;
            continue;
        }

        assert(storeWBIt->hasRequest());
        assert(!storeWBIt->committed());

        DynInstPtr inst = storeWBIt->instruction();
        LSQRequest* req = storeWBIt->request();

        // Process store conditionals or store release after all previous
        // stores are completed
        if ((req->mainRequest()->isLLSC() ||
             req->mainRequest()->isRelease()) &&
             (storeWBIt.idx() != storeQueue.head())) {
            DPRINTF(LSQUnit, "Store idx:%i PC:%s to Addr:%#x "
                "[sn:%lli] is %s%s and not head of the queue\n",
                storeWBIt.idx(), inst->pcState(),
                req->request()->getPaddr(), inst->seqNum,
                req->mainRequest()->isLLSC() ? "SC" : "",
                req->mainRequest()->isRelease() ? "/Release" : "");
            break;
        }

        storeWBIt->committed() = true;

        assert(!inst->memData);
        inst->memData = new uint8_t[req->_size];

        if (storeWBIt->isAllZeros())
            memset(inst->memData, 0, req->_size);
        else
            memcpy(inst->memData, storeWBIt->data(), req->_size);


        if (req->senderState() == nullptr) {
            SQSenderState *state = new SQSenderState(storeWBIt);
            state->isLoad = false;
            state->needWB = false;
            state->inst = inst;

            req->senderState(state);
            if (inst->isStoreConditional() || inst->isAtomic()) {
                /* Only store conditionals and atomics need a writeback. */
                state->needWB = true;
            }
        }
        req->buildPackets();

        DPRINTF(LSQUnit, "D-Cache: Writing back store idx:%i PC:%s "
                "to Addr:%#x, data:%#x [sn:%lli]\n",
                storeWBIt.idx(), inst->pcState(),
                req->request()->getPaddr(), (int)*(inst->memData),
                inst->seqNum);

        // @todo: Remove this SC hack once the memory system handles it.
        if (inst->isStoreConditional()) {
            // Disable recording the result temporarily.  Writing to
            // misc regs normally updates the result, but this is not
            // the desired behavior when handling store conditionals.
            inst->recordResult(false);
            bool success = TheISA::handleLockedWrite(inst.get(),
                    req->request(), cacheBlockMask);
            inst->recordResult(true);
            req->packetSent();

            if (!success) {
                req->complete();
                // Instantly complete this store.
                DPRINTF(LSQUnit, "Store conditional [sn:%lli] failed.  "
                        "Instantly completing it.\n",
                        inst->seqNum);
                PacketPtr new_pkt = new Packet(*req->packet());
                WritebackEvent *wb = new WritebackEvent(inst,
                        new_pkt, this);
                cpu->schedule(wb, curTick() + 1);
                completeStore(storeWBIt);
                if (!storeQueue.empty())
                    storeWBIt++;
                else
                    storeWBIt = storeQueue.end();
                continue;
            }
        }

        if (req->request()->isLocalAccess()) {
            assert(!inst->isStoreConditional());
            assert(!inst->inHtmTransactionalState());
            ThreadContext *thread = cpu->tcBase(lsqID);
            PacketPtr main_pkt = new Packet(req->mainRequest(),
                                            MemCmd::WriteReq);
            main_pkt->dataStatic(inst->memData);
            req->request()->localAccessor(thread, main_pkt);
            delete main_pkt;
            completeStore(storeWBIt);
            storeWBIt++;
            continue;
        }
        /* Send to cache */
        req->sendPacketToCache();

        /* If successful, do the post send */
        if (req->isSent()) {
            storePostSend();
        } else {
            DPRINTF(LSQUnit, "D-Cache became blocked when writing [sn:%lli], "
                    "will retry later\n",
                    inst->seqNum);
        }
    }
    assert(stores >= 0 && storesToWB >= 0);
}

template <class Impl>
void
LSQUnit<Impl>::squash(const InstSeqNum &squashed_num)
{
    DPRINTF(LSQUnit, "Squashing until [sn:%lli]!"
            "(Loads:%i Stores:%i)\n", squashed_num, loads, stores);

    while (loads != 0 &&
            loadQueue.back().instruction()->seqNum > squashed_num) {
        DPRINTF(LSQUnit,"Load Instruction PC %s squashed, "
                "[sn:%lli]\n",
                loadQueue.back().instruction()->pcState(),
                loadQueue.back().instruction()->seqNum);

        if (isStalled() && loadQueue.tail() == stallingLoadIdx) {
            stalled = false;
            stallingStoreIsn = 0;
            stallingLoadIdx = 0;
        }

        // hardware transactional memory
        // Squashing instructions can alter the transaction nesting depth
        // and must be corrected before fetching resumes.
        if (loadQueue.back().instruction()->isHtmStart())
        {
            htmStarts = (--htmStarts < 0) ? 0 : htmStarts;
            DPRINTF(HtmCpu, ">> htmStarts-- (%d) : htmStops (%d)\n",
              htmStarts, htmStops);
        }
        if (loadQueue.back().instruction()->isHtmStop())
        {
            htmStops = (--htmStops < 0) ? 0 : htmStops;
            DPRINTF(HtmCpu, ">> htmStarts (%d) : htmStops-- (%d)\n",
              htmStarts, htmStops);
        }
        if (cpu ->AP &&
            loadQueue.back().instruction()->isRanAhead())
            updateRunAhead(loadQueue.back().instruction()->instAddr(), -1);

        // Clear the smart pointer to make sure it is decremented.
        loadQueue.back().instruction()->setSquashed();
        loadQueue.back().clear();

        --loads;

        loadQueue.pop_back();
        ++stats.squashedLoads;
    }

    // hardware transactional memory
    // scan load queue (from oldest to youngest) for most recent valid htmUid
    auto scan_it = loadQueue.begin();
    uint64_t in_flight_uid = 0;
    while (scan_it != loadQueue.end()) {
        if (scan_it->instruction()->isHtmStart() &&
            !scan_it->instruction()->isSquashed()) {
            in_flight_uid = scan_it->instruction()->getHtmTransactionUid();
            DPRINTF(HtmCpu, "loadQueue[%d]: found valid HtmStart htmUid=%u\n",
                scan_it._idx, in_flight_uid);
        }
        scan_it++;
    }
    // If there's a HtmStart in the pipeline then use its htmUid,
    // otherwise use the most recently committed uid
    const auto& htm_cpt = cpu->tcBase(lsqID)->getHtmCheckpointPtr();
    if (htm_cpt) {
        const uint64_t old_local_htm_uid = htm_cpt->getHtmUid();
        uint64_t new_local_htm_uid;
        if (in_flight_uid > 0)
            new_local_htm_uid = in_flight_uid;
        else
            new_local_htm_uid = lastRetiredHtmUid;

        if (old_local_htm_uid != new_local_htm_uid) {
            DPRINTF(HtmCpu, "flush: lastRetiredHtmUid=%u\n",
                lastRetiredHtmUid);
            DPRINTF(HtmCpu, "flush: resetting localHtmUid=%u\n",
                new_local_htm_uid);

            htm_cpt->setHtmUid(new_local_htm_uid);
        }
    }

    if (memDepViolator && squashed_num < memDepViolator->seqNum) {
        memDepViolator = NULL;
    }

    while (stores != 0 &&
           storeQueue.back().instruction()->seqNum > squashed_num) {
        // Instructions marked as can WB are already committed.
        if (storeQueue.back().canWB()) {
            break;
        }

        DPRINTF(LSQUnit,"Store Instruction PC %s squashed, "
                "idx:%i [sn:%lli]\n",
                storeQueue.back().instruction()->pcState(),
                storeQueue.tail(), storeQueue.back().instruction()->seqNum);

        // I don't think this can happen.  It should have been cleared
        // by the stalling load.
        if (isStalled() &&
            storeQueue.back().instruction()->seqNum == stallingStoreIsn) {
            panic("Is stalled should have been cleared by stalling load!\n");
            stalled = false;
            stallingStoreIsn = 0;
        }

        // Clear the smart pointer to make sure it is decremented.
        storeQueue.back().instruction()->setSquashed();

        // Must delete request now that it wasn't handed off to
        // memory.  This is quite ugly.  @todo: Figure out the proper
        // place to really handle request deletes.
        storeQueue.back().clear();
        --stores;

        storeQueue.pop_back();
        ++stats.squashedStores;
    }
}

template <class Impl>
void
LSQUnit<Impl>::storePostSend()
{
    if (isStalled() &&
        storeWBIt->instruction()->seqNum == stallingStoreIsn) {
        DPRINTF(LSQUnit, "Unstalling, stalling store [sn:%lli] "
                "load idx:%i\n",
                stallingStoreIsn, stallingLoadIdx);
        stalled = false;
        stallingStoreIsn = 0;
        iewStage->replayMemInst(loadQueue[stallingLoadIdx].instruction());
    }

    if (!storeWBIt->instruction()->isStoreConditional()) {
        // The store is basically completed at this time. This
        // only works so long as the checker doesn't try to
        // verify the value in memory for stores.
        storeWBIt->instruction()->setCompleted();

        if (cpu->checker) {
            cpu->checker->verify(storeWBIt->instruction());
        }
    }

    if (needsTSO) {
        storeInFlight = true;
    }

    forwardToPredicted();

    storeWBIt++;
}

template <class Impl>
void
LSQUnit<Impl>::forwardToPredicted()
{
    DynInstPtr store_inst = storeWBIt->instruction();
    DPRINTF(AddrPredDebug, "Attempting to forward sent store to predInsts "
            "for [sn:%llu] with addr %#x, paddr %#x and size %d. "
            "Number of predInsts: %d\n",
            store_inst->seqNum, store_inst->effAddr,
            store_inst->physEffAddr, storeWBIt->size(),
            predInsts.size());
    for (int i = 0; i < predInsts.size(); i++) {
        DynInstPtr load_inst = predInsts.at(i);
        if (load_inst->isCommitted() || load_inst->isSquashed()) {
            predInsts.erase(predInsts.begin() + i);
            i--;
            continue;
        }
        if (load_inst->seqNum < store_inst->seqNum) {
            continue;
        }

        int store_size = storeWBIt->size();

        // Cache maintenance instructions go down via the store
        // path but they carry no data and they shouldn't be
        // considered for forwarding
        if (store_size != 0 && !storeWBIt->instruction()->strictlyOrdered() &&
            !(storeWBIt->request()->mainRequest() &&
              storeWBIt->request()->mainRequest()->isCacheMaintenance())) {
            assert(storeWBIt->instruction()->effAddrValid());

            // Check if the store data is within the lower and upper bounds of
            // addresses that the request needs.
            auto req_s = load_inst->getPredAddr();
            auto req_e = req_s + load_inst->getPredSize();
            auto st_s = store_inst->effAddr;
            auto st_e = st_s + store_size;

            bool store_has_lower_limit = req_s >= st_s;
            bool store_has_upper_limit = req_e <= st_e;
            bool lower_load_has_store_part = req_s < st_e;
            bool upper_load_has_store_part = req_e > st_s;

            auto coverage = AddrRangeCoverage::NoAddrRangeCoverage;

            // If the store entry is not atomic (atomic does not have valid
            // data), the store has all of the data needed, and
            // the load is not LLSC, then
            // we can forward data from the store to the load
            if (!storeWBIt->instruction()->isAtomic() &&
                store_has_lower_limit && store_has_upper_limit) {

                const auto& store_req = storeWBIt->request()->mainRequest();
                coverage = store_req->isMasked() ?
                    AddrRangeCoverage::PartialAddrRangeCoverage :
                    AddrRangeCoverage::FullAddrRangeCoverage;
            } else if (
                (storeWBIt->instruction()->isAtomic() &&
                 ((store_has_lower_limit || upper_load_has_store_part) &&
                  (store_has_upper_limit || lower_load_has_store_part)))) {

                panic("LLSC/atomic stores should never be linked here");
            } else if (
                // This is the partial store-load forwarding case where a store
                // has only part of the load's data and the load isn't LLSC
                ((store_has_lower_limit && lower_load_has_store_part) ||
                  (store_has_upper_limit && upper_load_has_store_part) ||
                  (lower_load_has_store_part && upper_load_has_store_part))) {

                coverage = AddrRangeCoverage::PartialAddrRangeCoverage;
            }

            if (coverage == AddrRangeCoverage::FullAddrRangeCoverage) {
                // Get shift amount for offset into the store's data.
                int shift_amt = req_s - st_s;

                // Allocate memory if this is the first time a load is issued.
                if (!load_inst->storeData) {
                    load_inst->storeData =
                        new uint8_t[load_inst->getPredSize()];
                    memset(load_inst->storeData, 0, load_inst->getPredSize());

                }
                if (storeWBIt->isAllZeros())
                    memset(load_inst->storeData, 0,
                            load_inst->getPredSize());
                else
                    memcpy(load_inst->storeData,
                        storeWBIt->data() + shift_amt,
                        load_inst->getPredSize());

                DPRINTF(LSQUnit, "Forwarding from store idx %i to pred load "
                        "for addr %#x for load_inst [sn:%llu]\n",
                        storeWBIt._idx,
                        req_s,
                        load_inst->seqNum);
                ++stats.forwardedToPredictions;
                load_inst->hasStoreData = true;
            } else if (coverage ==
                        AddrRangeCoverage::PartialAddrRangeCoverage) {
                load_inst->partialStoreConflict = true;
                ++stats.partialPredStoreConflicts;
            }
        }
    }
}

template <class Impl>
void
LSQUnit<Impl>::writeback(const DynInstPtr &inst, PacketPtr pkt)
{
    iewStage->wakeCPU();

    // Squashed instructions do not need to complete their access.
    if (inst->isSquashed()) {
        assert (!inst->isStore() || inst->isStoreConditional());
        ++stats.ignoredResponses;
        return;
    }

    if (!inst->isExecuted()) {
        inst->setExecuted();

        if (inst->fault == NoFault) {
            // Complete access to copy data to proper place.
            inst->completeAcc(pkt);
        } else {
            // If the instruction has an outstanding fault, we cannot complete
            // the access as this discards the current fault.

            // If we have an outstanding fault, the fault should only be of
            // type ReExec or - in case of a SplitRequest - a partial
            // translation fault

            // Unless it's a hardware transactional memory fault
            auto htm_fault = std::dynamic_pointer_cast<
                GenericHtmFailureFault>(inst->fault);

            if (!htm_fault) {
                assert(dynamic_cast<ReExec*>(inst->fault.get()) != nullptr ||
                       inst->savedReq->isPartialFault());

            } else if (!pkt->htmTransactionFailedInCache()) {
                // Situation in which the instruction has a hardware transactional
                // memory fault but not the packet itself. This can occur with
                // ldp_uop microops since access is spread over multiple packets.
                DPRINTF(HtmCpu,
                        "%s writeback with HTM failure fault, "
                        "however, completing packet is not aware of "
                        "transaction failure. cause=%s htmUid=%u\n",
                        inst->staticInst->getName(),
                        htmFailureToStr(htm_fault->getHtmFailureFaultCause()),
                        htm_fault->getHtmUid());
            }

            DPRINTF(LSQUnit, "Not completing instruction [sn:%lli] access "
                    "due to pending fault.\n", inst->seqNum);
        }
    }

    // Need to insert instruction into queue to commit
    iewStage->instToCommit(inst);

    iewStage->activityThisCycle();

    // see if this load changed the PC
    iewStage->checkMisprediction(inst);
}

template <class Impl>
void
LSQUnit<Impl>::completeStore(typename StoreQueue::iterator store_idx)
{
    assert(store_idx->valid());
    store_idx->completed() = true;
    --storesToWB;
    // A bit conservative because a store completion may not free up entries,
    // but hopefully avoids two store completions in one cycle from making
    // the CPU tick twice.
    cpu->wakeCPU();
    cpu->activityThisCycle();

    /* We 'need' a copy here because we may clear the entry from the
     * store queue. */
    DynInstPtr store_inst = store_idx->instruction();
    if (store_idx == storeQueue.begin()) {
        do {
            storeQueue.front().clear();
            storeQueue.pop_front();
            --stores;
        } while (storeQueue.front().completed() &&
                 !storeQueue.empty());

        iewStage->updateLSQNextCycle = true;
    }

    DPRINTF(LSQUnit, "Completing store [sn:%lli], idx:%i, store head "
            "idx:%i\n",
            store_inst->seqNum, store_idx.idx() - 1, storeQueue.head() - 1);

#if TRACING_ON
    if (DTRACE(O3PipeView)) {
        store_inst->storeTick =
            curTick() - store_inst->fetchTick;
    }
#endif

    if (isStalled() &&
        store_inst->seqNum == stallingStoreIsn) {
        DPRINTF(LSQUnit, "Unstalling, stalling store [sn:%lli] "
                "load idx:%i\n",
                stallingStoreIsn, stallingLoadIdx);
        stalled = false;
        stallingStoreIsn = 0;
        iewStage->replayMemInst(loadQueue[stallingLoadIdx].instruction());
    }

    store_inst->setCompleted();

    if (needsTSO) {
        storeInFlight = false;
    }

    // Tell the checker we've completed this instruction.  Some stores
    // may get reported twice to the checker, but the checker can
    // handle that case.
    // Store conditionals cannot be sent to the checker yet, they have
    // to update the misc registers first which should take place
    // when they commit
    if (cpu->checker &&  !store_inst->isStoreConditional()) {
        cpu->checker->verify(store_inst);
    }
}

template <class Impl>
void
LSQUnit<Impl>::updateRunAhead(Addr addr, int update)
{
    if (runAhead.find(addr) != runAhead.end()) {
        runAhead[addr] = runAhead[addr] + update;
    } else {
        assert(update > 0);
        runAhead[addr] = 1;
    }
}


template <class Impl>
bool
LSQUnit<Impl>::fireAndForget(PacketPtr data_pkt, DynInstPtr load_inst)
{
    assert(data_pkt->isRead());
    if (lsq->cacheBlocked() || (!lsq->cachePortAvailable(true))) {
        ++stats.blockedPredictedPreloads;
        delete(data_pkt);
        return false;
    }
    lsq->cachePortBusy(true);
    auto missed = snoopCache(data_pkt->getAddr(), load_inst);
    DPRINTF(DOM, "Firing and forgetting, with miss %d\n", missed);
    if (missed) {
        ++stats.predictedPreloads;
        auto ret = dcachePort->sendTimingReq(data_pkt);
        return ret;
    } else {
        ++stats.predictedHits;
        delete(data_pkt);
        return true;
    }
}

template <class Impl>
bool
LSQUnit<Impl>::trySendPacket(bool isLoad, PacketPtr data_pkt)
{
    assert(data_pkt);
    bool ret = true;
    bool cache_got_blocked = false;

    auto state = dynamic_cast<LSQSenderState*>(data_pkt->senderState);

    if (!lsq->cacheBlocked() &&
        lsq->cachePortAvailable(isLoad)) {
        if (!dcachePort->sendTimingReq(data_pkt)) {
            ret = false;
            cache_got_blocked = true;
        }
    } else {
        ret = false;
    }

    if (ret) {
        if (!isLoad) {
            isStoreBlocked = false;
        }
        lsq->cachePortBusy(isLoad);
        state->outstanding++;
        state->request()->packetSent();
    } else {
        if (cache_got_blocked) {
            lsq->cacheBlocked(true);
            ++stats.blockedByCache;
        }
        if (!isLoad) {
            assert(state->request() == storeWBIt->request());
            isStoreBlocked = true;
        }
        state->request()->packetNotSent();
    }
    return ret;
}

template <class Impl>
void
LSQUnit<Impl>::recvRetry()
{
    if (isStoreBlocked) {
        DPRINTF(LSQUnit, "Receiving retry: blocked store\n");
        writebackBlockedStore();
    }
}

template <class Impl>
void
LSQUnit<Impl>::dumpInsts() const
{
    cprintf("Load store queue: Dumping instructions.\n");
    cprintf("Load queue size: %i\n", loads);
    cprintf("Load queue: ");

    for (const auto& e: loadQueue) {
        const DynInstPtr &inst(e.instruction());
        cprintf("%s.[sn:%llu] ", inst->pcState(), inst->seqNum);
    }
    cprintf("\n");

    cprintf("Store queue size: %i\n", stores);
    cprintf("Store queue: ");

    for (const auto& e: storeQueue) {
        const DynInstPtr &inst(e.instruction());
        cprintf("%s.[sn:%llu] ", inst->pcState(), inst->seqNum);
    }

    cprintf("\n");
}

template <class Impl>
unsigned int
LSQUnit<Impl>::cacheLineSize()
{
    return cpu->cacheLineSize();
}


template<class Impl>
bool
LSQUnit<Impl>::forwardPredictedData(const DynInstPtr& load_inst)
{
    DPRINTF(AddrPredDebug, "Forwarding predicted data for "
            "load_inst [sn:%llu]\n", load_inst->seqNum);

    // Allocate memory if this is the first time a load is issued.
    assert(!load_inst->isSquashed());
    assert(!load_inst->hasStoreData);
    ++stats.forwardedPredictedData;

    LSQRequest *req = load_inst->savedReq;

    if (!load_inst->memData) {
        load_inst->memData =
            new uint8_t[req->mainRequest()->getSize()];
    }
    if (!verifyLoadDataIntegrity(load_inst))
        ++stats.incorrectPredData;

    memcpy(load_inst->memData,
           load_inst->verifyData,
           req->mainRequest()->getSize());

    DPRINTF(LSQUnit, "Forwarding from predAddr to load for "
            "inst [sn:%llu], addr: %#x, size: %d\n",
            load_inst->seqNum,
            req->mainRequest()->getVaddr(),
            req->mainRequest()->getSize());

    PacketPtr data_pkt = new Packet(req->mainRequest(),
            MemCmd::ReadReq);
    data_pkt->dataStatic(load_inst->memData);

    WritebackEvent *wb = new WritebackEvent(load_inst, data_pkt,
            this);

    // We'll say this has a 1 cycle load-store forwarding latency
    // for now.
    // @todo: Need to make this a parameter.
    cpu->schedule(wb, curTick());

    return true;
}

template<class Impl>
bool
LSQUnit<Impl>::forwardStoredData(const DynInstPtr& load_inst)
{
    DPRINTF(AddrPredDebug, "Forwarding store data for "
            "load_inst [sn:%llu]\n", load_inst->seqNum);

    assert(!load_inst->isSquashed());
    ++stats.forwardedStoreData;

    LSQRequest *req = load_inst->savedReq;

    if (!load_inst->memData) {
        load_inst->memData =
            new uint8_t[req->mainRequest()->getSize()];
        memset(load_inst->memData, 0, req->mainRequest()->getSize());
    }

    if (!verifyLoadDataIntegrity(load_inst))
        ++stats.incorrectStoredData;

    memcpy(load_inst->memData,
           load_inst->verifyData,
           req->mainRequest()->getSize());

    DPRINTF(LSQUnit, "Forwarding from predAddr store"
            " to load for inst [sn:%llu], addr %#x, size: %d\n",
            load_inst->seqNum,
            req->mainRequest()->getVaddr(),
            req->mainRequest()->getSize());

    PacketPtr data_pkt = new Packet(req->mainRequest(),
                                    MemCmd::ReadReq);

    data_pkt->dataStatic(load_inst->memData);

    WritebackEvent *wb = new WritebackEvent(load_inst, data_pkt, this);

    cpu->schedule(wb, curTick());

    return true;
}

template<class Impl>
void
LSQUnit<Impl>::addToPredInsts(const DynInstPtr& load_inst)
{
    predInsts.push_back(load_inst);
}

template<class Impl>
void
LSQUnit<Impl>::cleanPredInsts()
{
    for (int i = 0; i < predInsts.size(); i++) {
        DynInstPtr load_inst = predInsts.at(i);
        if (load_inst->isCommitted() || load_inst->isSquashed()) {
            predInsts.erase(predInsts.begin() + i);
            i--;
            continue;
        }
    }
}

template<class Impl>
bool
LSQUnit<Impl>::verifyLoadDataIntegrity(const DynInstPtr& load_inst)
{
    assert(load_inst->hasStoreData || load_inst->hasPredData);

    int size = load_inst->savedReq->mainRequest()->getSize();

    if (!load_inst->verifyData){
        load_inst->verifyData = new uint8_t[size];
        memset(load_inst->verifyData, 0, size);
    }

    LSQRequest *req = load_inst->savedReq;
    PacketPtr snoop = Packet::createRead(req->mainRequest());

    snoop->dataStatic(load_inst->verifyData);
    snoop->verificationSpeculativeMode();
    LSQSenderState *state = new LQSnoopState(req);
    snoop->senderState = state;
    dcachePort->sendFunctional(snoop);



    DPRINTF(AddrPredDebug, "Verifying load data integrity for "
            "inst [sn:%llu] with paddr %llx and size: %d. \n"
            "pred: %#x:%#x:%#x:%#x:%#x:%#x:%#x:%#x\n"
            "real: %#x:%#x:%#x:%#x:%#x:%#x:%#x:%#x\n",
            load_inst->seqNum,
            load_inst->physEffAddr,
            size,
            load_inst->predData[0],
            load_inst->predData[1],
            load_inst->predData[2],
            load_inst->predData[3],
            load_inst->predData[4],
            load_inst->predData[5],
            load_inst->predData[6],
            load_inst->predData[7],
            load_inst->verifyData[0],
            load_inst->verifyData[1],
            load_inst->verifyData[2],
            load_inst->verifyData[3],
            load_inst->verifyData[4],
            load_inst->verifyData[5],
            load_inst->verifyData[6],
            load_inst->verifyData[7]);
    if (load_inst->hasStoreData)
        DPRINTF(AddrPredDebug,
            "stor: %#x:%#x:%#x:%#x:%#x:%#x:%#x:%#x\n",
            load_inst->storeData[0],
            load_inst->storeData[1],
            load_inst->storeData[2],
            load_inst->storeData[3],
            load_inst->storeData[4],
            load_inst->storeData[5],
            load_inst->storeData[6],
            load_inst->storeData[7]);

    bool equal = true;

    for (int i = 0; i < size; i++) {
        equal = equal && (load_inst->verifyData[i] ==
            (load_inst->hasStoreData ?
                load_inst->storeData[i] : load_inst->predData[i]));
    }

    delete(snoop);
    delete(state);

    return equal;
}

template<class Impl>
bool
LSQUnit<Impl>::snoopCache(LSQRequest *req, const DynInstPtr& load_inst)
{
    PacketPtr ex_snoop = Packet::createRead(req->mainRequest());
    ex_snoop->dataStatic(load_inst->memData);
    ex_snoop->setExpressSnoop();
    ex_snoop->speculative = req->speculative;
    ex_snoop->domSpeculativeMode();
    LSQSenderState *state = new LQSnoopState(req);
    ex_snoop->senderState = state;
    dcachePort->sendFunctional(ex_snoop);
    ++stats.issuedSnoops;

    auto missed = ex_snoop->isCacheMiss();
    delete(ex_snoop);
    delete(state);
    DPRINTF(DOM, "Issued snoop to cache"
        "Missed: %d for [sn:%llu] \n", missed, load_inst->seqNum);
    return missed;
}

template<class Impl>
bool
LSQUnit<Impl>::snoopCache(Addr target, const DynInstPtr& load_inst)
{
    Request::Flags _flags = Request::PHYSICAL;
    auto request = std::make_shared<Request>(
                        target,
                        8,//inst->effSize,
                        _flags,
                        load_inst->requestorId());
    PacketPtr snoop = new Packet(request, MemCmd::ReadReq);
    snoop->dataStatic(load_inst->memData);
    snoop->mpspemSpeculativeMode();
    snoop->speculative = true;
    dcachePort->sendFunctional(snoop);
    ++stats.issuedSnoops;

    auto missed = snoop->isCacheMiss();
    delete(snoop);
    return missed;
}


#endif//__CPU_O3_LSQ_UNIT_IMPL_HH__
