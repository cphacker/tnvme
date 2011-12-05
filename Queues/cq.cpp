#include "cq.h"
#include "globals.h"
#include "../Utils/kernelAPI.h"

#define NUM_TIME_SEGMENTS      10


CQ::CQ() :
    Queue(0, Trackable::OBJTYPE_FENCE)
{
    // This constructor will throw
}


CQ::CQ(int fd, Trackable::ObjType objBeingCreated) :
    Queue(fd, objBeingCreated)
{
    mIrqEnabled = false;
    mIrqVec = 0;
}


CQ::~CQ()
{
    try {
        // Cleanup duties for this Q's buffer
        if (GetIsContig()) {
            // Contiguous memory is alloc'd and owned by the kernel
            KernelAPI::munmap(mContigBuf, GetQSize());
        }
    } catch (...) {
        ;   // Destructors should never throw. If the object is deleted B4
            // it is Init'd() properly, it could throw, so catch and ignore
    }
}


void
CQ::Init(uint16_t qId, uint16_t entrySize, uint16_t numEntries,
    bool irqEnabled, uint16_t irqVec)
{
    Queue::Init(qId, entrySize, numEntries);
    mIrqEnabled = irqEnabled;
    mIrqVec = irqVec;


    LOG_NRM("Allocating contiguous CQ memory in dnvme");
    if (GetIsAdmin()) {
        if (gCtrlrConfig->IsStateEnabled()) {
            // At best this will cause tnvme to seg fault or a kernel crash
            // The NVME spec states unpredictable outcomes will occur.
            LOG_DBG("Creating an ASQ while ctrlr is enabled is a shall not");
            throw exception();
        }

        // We are creating a contiguous ACQ. ACQ's have a constant well known
        // element size and no setup is required for this type of Q.
        int ret;
        struct nvme_create_admn_q q;
        q.elements = GetNumEntries();
        q.type = ADMIN_CQ;

        LOG_NRM("Init contig ACQ: (id, entrySize, numEntries) = (%d, %d, %d)",
            GetQId(), GetEntrySize(), GetNumEntries());

        if ((ret = ioctl(mFd, NVME_IOCTL_CREATE_ADMN_Q, &q)) < 0) {
            LOG_DBG("Q Creation failed by dnvme with error: 0x%02X", ret);
            throw exception();
        }
    } else {
        // We are creating a contiguous IOCQ.
        struct nvme_prep_cq q;
        q.cq_id = GetQId();
        q.elements = GetNumEntries();
        q.contig = true;
        CreateIOCQ(q);
    }

    // Contiguous Q's are created in dnvme and must be mapped back to user space
    mContigBuf = KernelAPI::mmap(mFd, GetQSize(), GetQId(), KernelAPI::MMR_CQ);
    if (mContigBuf == NULL) {
        LOG_DBG("Unable to mmap contig memory to user space");
        throw exception();
    }
}


void
CQ::Init(uint16_t qId, uint16_t entrySize, uint16_t numEntries,
    const SharedMemBufferPtr memBuffer, bool irqEnabled, uint16_t irqVec)
{
    Queue::Init(qId, entrySize, numEntries);
    mIrqEnabled = irqEnabled;
    mIrqVec = irqVec;


    LOG_NRM("Allocating discontiguous CQ memory in tnvme");
    if (memBuffer == MemBuffer::NullMemBufferPtr) {
        LOG_DBG("Passing an uninitialized SharedMemBufferPtr");
        throw exception();
    } else if (GetIsAdmin()) {
        // There are no appropriate methods for an NVME device to report ASC/ACQ
        // creation errors, thus since ASC/ASQ may only be contiguous then don't
        // allow these problems to be injected, at best they will only succeed
        // to seg fault the app or crash the kernel.
        LOG_DBG("Illegal memory alignment will corrupt");
        throw exception();
    } else  if (memBuffer->GetBufSize() < GetQSize()) {
        LOG_DBG("Q buffer memory ambiguous to passed size params");
        LOG_DBG("Mem buffer size = %d, Q size = %d", memBuffer->GetBufSize(),
            GetQSize());
        throw exception();
    } else if (memBuffer->GetAlignment() != sysconf(_SC_PAGESIZE)) {
        // Nonconformance to page alignment will seg fault the app or crash
        // the kernel. This state is not testable since no errors can be
        // reported by hdw, thus disallow this attempt.
        LOG_DBG("Q content memory shall be page aligned");
        throw exception();
    }

    // Zero out the content memory so the P-bit correlates to a newly alloc'd Q.
    // Also assuming life time ownership of this object if it wasn't created
    // by the RsrcMngr.
    mDiscontigBuf = memBuffer;
    mDiscontigBuf->Zero();

    // We are creating a discontiguous IOCQ
    struct nvme_prep_cq q;
    q.cq_id = GetQId();
    q.elements = GetNumEntries();
    q.contig = false;
    CreateIOCQ(q);
}


void
CQ::CreateIOCQ(struct nvme_prep_cq &q)
{
    int ret;

    LOG_NRM("Init %s CQ: (id, entrySize, numEntries) = (%d, %d, %d)",
        q.contig ? "contig" : "discontig", GetQId(), GetEntrySize(),
        GetNumEntries());

    if ((ret = ioctl(mFd, NVME_IOCTL_PREPARE_CQ_CREATION, &q)) < 0) {
        LOG_DBG("Q Creation failed by dnvme with error: 0x%02X", ret);
        throw exception();
    }
}


struct nvme_gen_cq
CQ::GetQMetrics()
{
    int ret;
    struct nvme_gen_cq qMetrics;
    struct nvme_get_q_metrics getQMetrics;

    getQMetrics.q_id = GetQId();
    getQMetrics.type = METRICS_CQ;
    getQMetrics.nBytes = GetQSize();
    getQMetrics.buffer = (uint8_t *)&qMetrics;

    if ((ret = ioctl(mFd, NVME_IOCTL_GET_Q_METRICS, &getQMetrics)) < 0) {
        LOG_DBG("Get Q metrics failed by dnvme with error: 0x%02X", ret);
        throw exception();
    }
    return qMetrics;
}


union CE
CQ::PeekCE(uint16_t indexPtr)
{
    union CE *dataPtr;

    if (GetIsContig())
        dataPtr = (union CE *)mContigBuf;
    else
        dataPtr = (union CE *)mDiscontigBuf->GetBuffer();

    for (int i = 0; i < GetNumEntries(); i++, dataPtr++) {
        if (i == indexPtr)
            return *dataPtr;
    }

    LOG_DBG("Unable to locate index within Q");
    throw exception();
}


void
CQ::LogCE(uint16_t indexPtr)
{
    union CE ce = PeekCE(indexPtr);
    LOG_NRM("Logging Completion Element (CE)...");
    LOG_NRM("CQ %d, CE %d, DWORD0: 0x%08X", GetQId(), indexPtr, ce.d.dw0);
    LOG_NRM("CQ %d, CE %d, DWORD1: 0x%08X", GetQId(), indexPtr, ce.d.dw1);
    LOG_NRM("CQ %d, CE %d, DWORD2: 0x%08X", GetQId(), indexPtr, ce.d.dw2);
    LOG_NRM("CQ %d, CE %d, DWORD3: 0x%08X", GetQId(), indexPtr, ce.d.dw3);
}


uint16_t
CQ::ReapInquiry()
{
    int rc;
    struct nvme_reap_inquiry inq;

    inq.q_id = GetQId();
    if ((rc = ioctl(mFd, NVME_IOCTL_REAP_INQUIRY, &inq)) < 0) {
        LOG_ERR("Error during reap inquiry, rc =%d", rc);
        throw exception();
    }

    LOG_NRM("%d CE's awaiting attention in SQ %d", inq.num_remaining, inq.q_id);
    return inq.num_remaining;
}


bool
CQ::ReapInquiryWaitAny(uint16_t ms, uint16_t &numCE)
{
    // Chunk the wait period up into equal segments, until such time there
    // can be time to develop a select() solution in dnvme
    useconds_t segments = ((ms * 1000) / NUM_TIME_SEGMENTS);

    for (int i = 0; i < NUM_TIME_SEGMENTS; i++) {
        if ((numCE = ReapInquiry()) != 0) {
            return true;
        }
        usleep(segments);
    }
    return false;
}


bool
CQ::ReapInquiryWaitSpecify(uint16_t ms, uint16_t numTil, uint16_t &numCE)
{
    // Chunk the wait period up into equal segments, until such time there
    // can be time to develop a select() solution in dnvme
    useconds_t segments = ((ms * 1000) / NUM_TIME_SEGMENTS);

    for (int i = 0; i < NUM_TIME_SEGMENTS; i++) {
        if ((numCE = ReapInquiry()) != 0) {
            if (numCE >= numTil)
                return true;
        }
        usleep(segments);
    }
    return false;
}


uint16_t
CQ::Reap(uint16_t &ceRemain, SharedMemBufferPtr memBuffer,
    uint16_t ceDesire, bool zeroMem)
{
    int rc;
    struct nvme_reap reap;

    // The tough part of reaping all which can be reaped, indicated by
    // (ceDesire == 0), is that CE's can be arriving from hdw between the time
    // one calls ReapInquiry() and Reap(). In essence this indicates we really
    // can never know for certain how many there are to be reaped, and thus
    // never really knowing how large to make a buffer to reap CE's into.
    // The solution is to enforce brute force methods by allocating max CE's
    if (ceDesire == 0) {
        // Per NVME spec: 1 empty CE implies a full CQ, can't truly fill all
        ceDesire = (GetNumEntries() - 1);
    } else if (ceDesire > (GetNumEntries() - 1)) {
        // Per NVME spec: 1 empty CE implies a full CQ, can't truly fill all
        LOG_NRM("Requested num of CE's exceeds max can fit, resizing");
        ceDesire = (GetNumEntries() - 1);
    }
    LOG_NRM("Reaping %d CE's from CQ %d", ceDesire, GetQId());

    // Allocate enough space to contain the CE's
    memBuffer->Init(GetEntrySize()*ceDesire);
    if (zeroMem)
        memBuffer->Zero();

    reap.q_id = GetQId();
    reap.elements = ceDesire;
    reap.size = memBuffer->GetBufSize();
    reap.buffer = memBuffer->GetBuffer();
    if ((rc = ioctl(mFd, NVME_IOCTL_REAP, &reap)) < 0) {
        LOG_ERR("Error during reaping CE's, rc =%d", rc);
        throw exception();
    }

    ceRemain = reap.num_remaining;
    return reap.num_reaped;
}

