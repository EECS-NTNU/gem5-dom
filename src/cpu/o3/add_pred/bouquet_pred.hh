
#ifndef __CPU_PRED_ADD_PRED_BOUQUET_PRED_HH__
#define __CPU_PRED_ADD_PRED_BOUQUET_PRED_CC__

#include "arch/registers.hh"
#include "arch/types.hh"
#include "base/statistics.hh"
#include "base/stats/group.hh"
#include "base/types.hh"
#include "config/the_isa.hh"
#include "cpu/inst_seq.hh"
#include "params/DerivO3CPU.hh"
#include "sim/sim_object.hh"

struct DerivO3CPUParams;


// IP table entries
#define NUM_IP_TABLE_L1_ENTRIES 1024
// Entries in the GHB
#define NUM_GHB_ENTRIES 16
// Bits to index into the IP table
#define NUM_IP_INDEX_BITS 10
// Tag bits per IP table entry
#define NUM_IP_TAG_BITS 6
// stream
#define S_TYPE 1
// constant stride
#define CS_TYPE 2
// complex stride
#define CPLX_TYPE 3
// next line
#define NL_TYPE 4
#define NUM_CPUS 1
#define LOG2_PAGE_SIZE = 16;
#define LOG2_BLOCK_SIZE = 6;

class IP_TABLE_L1 {
    public:
        uint64_t ip_tag;
        // last page seen by IP
        uint64_t last_page;
        // last cl offset in the 4KB page
        uint64_t last_cl_offset;
        // last delta observed
        int64_t last_stride;
        // Valid IP or not
        uint16_t ip_valid;
        // CS conf
        int conf;
        // CPLX signature
        uint16_t signature;
        // stream direction
        uint16_t str_dir;
        // stream valid
        uint16_t str_valid;
        // stream strength
        uint16_t str_strength;

        int size;

        IP_TABLE_L1 () {
            ip_tag = 0;
            last_page = 0;
            last_cl_offset = 0;
            last_stride = 0;
            ip_valid = 0;
            signature = 0;
            conf = 0;
            str_dir = 0;
            str_valid = 0;
            str_strength = 0;
            size = 0;
        };
};

class DELTA_PRED_TABLE {
    public:
        int delta;
        int conf;

        DELTA_PRED_TABLE () {
            delta = 0;
            conf = 0;
        };
};

template<class Impl>
class BouquetPred
{
    public:

        BouquetPred(const DerivO3CPUParams &params);

        virtual ~BouquetPred();

        Addr predictFromPC(Addr pc, int runAhead);

        void updatePredictor(Addr realAddr, Addr pc,
                            InstSeqNum seqNum, int packetSize);

        void l1d_prefetcher_operate(uint64_t addr, uint64_t ip,
                                    int packetSize);

        int getPacketSize(Addr pc);

        int numEntries = 1024;

        const std::string name() const;

        uint16_t update_sig_l1(uint16_t old_sig, int delta);

        uint32_t encode_metadata(int stride, uint16_t type, int spec_nl);

        void check_for_stream_l1(int index, uint64_t cl_addr, uint8_t cpu);

        int update_conf(int stride, int pred_stride, int conf);

        IP_TABLE_L1 trackers_l1[NUM_IP_TABLE_L1_ENTRIES];
        DELTA_PRED_TABLE DPT_l1[4096];
        uint64_t ghb_l1[NUM_GHB_ENTRIES];
        uint64_t prev_cpu_cycle;
        uint64_t num_misses;
        float mpkc = 0;
        int spec_nl = 0;

};

#endif //__CPU_PRED_ADD_PRED_DELTA_PRED_HH__
