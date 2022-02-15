
#ifndef __CPU_PRED_ADD_PRED_BOUQUET_PRED_HH__
#define __CPU_PRED_ADD_PRED_BOUQUET_PRED_CC__

#include "base/flags.hh"
#include "base/types.hh"
#include "cpu/o3/add_pred/base_add_pred.hh"

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

class BouquetPred : virtual public BaseAddPred
{
    public:

        BouquetPred(const Params &params);

        virtual ~BouquetPred();

        Addr predictFromPC(Addr pc, int runAhead);

        void updatePredictor(Addr realAddr, Addr pc,
                            InstSeqNum seqNum, int packetSize);

        int getPacketSize(Addr pc);

        int numEntries = 1024;

        const std::string name() const;

        uint16_t update_sig_l1(uint16_t old_sig, int delta);

        uint32_t encode_metadata(int stride, uint16_t type, int spec_nl);

        void check_for_stream_l1(int index, uint64_t cl_addr, uint8_t cpu);

        int update_conf(int stride, int pred_stride, int conf);

        void l1d_prefetcher_operate(uint64_t addr, uint64_t ip);

        IP_TABLE_L1 trackers_l1[NUM_CPUS][NUM_IP_TABLE_L1_ENTRIES];
        DELTA_PRED_TABLE DPT_l1[NUM_CPUS][4096];
        uint64_t ghb_l1[NUM_CPUS][NUM_GHB_ENTRIES];
        uint64_t prev_cpu_cycle[NUM_CPUS];
        uint64_t num_misses[NUM_CPUS];
        float mpkc[NUM_CPUS] = {0};
        int spec_nl[NUM_CPUS] = {0};

};

#endif //__CPU_PRED_ADD_PRED_DELTA_PRED_HH__
