#ifndef __insens_packet_h__
#define __insens_packet_h__

// ======================================================================
//  Packet Formats: request, feedback, update
// ======================================================================

#define INSENS_REQUEST	0x01
#define INSENS_FEEDBACK	0x02
#define INSENS_UPDATE	0x03

// ======================================================================
// Direct access to packet headers
// ======================================================================

#define HDR_INSENS(p) ((struct hdr_insens *) hdr_insens::access(p))
#define HDR_INSENS_REQUEST(p)	((struct hdr_insens_request *) hdr_insens::access(p))
#define HDR_INSENS_FEEDBACK(p)	((struct hdr_insens_feedback *) hdr_insens::access(p))
#define HDR_INSENS_UPDATE(p)	((struct hdr_insens_update *) hdr_insens::access(p))

struct hops_path{
    u_int32_t hopid;
    hops_path *next;
};

struct path_info {
    u_int32_t hopid;
    struct hops_path {
        u_int32_t hopid;
        hops_path *next;
    }*hop_path;
    u_int16_t macr;
    inline int 	size() {
        int sz = 0;
        sz = sizeof(struct path_info);
        assert(sz>=0);
        return sz;
    }
};

struct nbr_info {
    nsaddr_t hopid;
    u_int16_t macr;
    nbr_info *next;
    
    inline int 	size() {
        int sz = 0;
        sz = sizeof(struct nbr_info);
        assert(sz>=0);
        return sz;
    }
};

// ======================================================================
// Default INSENS packet
// ======================================================================

struct hdr_insens {
	u_int8_t	pkt_type;

	// header access
	static int offset_;
	inline static int& offset() { return offset_;}
	inline static hdr_insens* access(const Packet *p) {
		return (hdr_insens*) p->access(offset_);
	}
};

// =====================================================================
// Request Packet Format
// =====================================================================

struct hdr_insens_request {
	
    u_int8_t 	pkt_type;   // type of packet
	u_int16_t 	pkt_ows;	// one-way function
    
    double      star_rreq;  // start time by sink
    
    double      timestart;
    double      delay;
    
    u_int8_t	beacon_hops;  // hop count, increadecreases as beacon is forwarded
    u_int32_t	beacon_id;   // unique identifier for the beacon
    nsaddr_t	beacon_src;  // source address of beacon, this is sink address
    
    double		timestamp;   // emission time of beacon message
    
    u_int32_t	posx; // x position of beacon source, if available
    u_int32_t	posy; // y position of beacon source, if available
    
	hops_path *hop_path;
	
    u_int16_t 	pkt_macr;	// message authentication code

	inline int 	size() {
		int sz = 0;
		sz = sizeof(struct hdr_insens_request);
		assert(sz>=0);
		return sz;
	}
};

// =====================================================================
// Feedback Packet Format
// =====================================================================

struct hdr_insens_feedback {
	u_int8_t 	pkt_type;   // type of packet
	u_int16_t 	pkt_ows;	// one-way function
	u_int16_t	pkt_macr;	// parent_info
    
    double      timestart;
    double      delay;

    nbr_info    *nbr;
    
    u_int8_t	beacon_hops;  // hop count, increadecreases as beacon is forwarded
    u_int32_t	beacon_id;   // unique identifier for the beacon
    nsaddr_t	beacon_src;  // source address of beacon, this is sink address
    
    nsaddr_t    prev_hop;
    
    u_int32_t	posx; // x position of beacon source, if available
    u_int32_t	posy; // y position of beacon source, if available
    
    double		timestamp;   // emission time of beacon message
    
	path_info *pathinfo;
	nbr_info *nbrinfo;
    inline int 	size() {
        int sz = 0;
        sz = sizeof(struct hdr_insens_feedback);
        assert(sz>=0);
        return sz;
    }
};

// =====================================================================
// Update Packet Format
// =====================================================================

struct hdr_insens_update {
	u_int8_t 	pkt_type;   // type of packet
	u_int16_t 	pkt_ows;	// one-way function
	u_int16_t	pkt_macr;	// message
    
    u_int8_t	beacon_hops;  // hop count, increadecreases as beacon is forwarded
    u_int32_t	beacon_id;   // unique identifier for the beacon
    nsaddr_t	beacon_src;  // source address of beacon, this is sink address
    
    double		timestamp;   // emission time of beacon message
    
	inline int 	size() {
		int sz = 0;
		sz = sizeof(struct hdr_insens_update);
		assert(sz>=0);
		return sz;
	}
};

// For size calculation of header-space reservation
union hdr_all_insens {
    hdr_insens		insens;
    hdr_insens_request		request;
    hdr_insens_feedback		feedback;
    hdr_insens_update		update;
};

#endif /* __insens_packet_h__ */
