#ifndef __insens_h__
#define __insens_h__

#include <cmu-trace.h>
#include <priqueue.h>
#include <classifier/classifier-port.h>
#include <insens/insens_packet.h>

#define DISTANCE 20
#define VC  300000000
#define ACCURACY    0.04
#define NETWORK_DIAMETER		64
#define DEFAULT_BEACON_INTERVAL		100 // seconds;
#define DEFAULT_ROUTE_EXPIRE 		2*DEFAULT_BEACON_INTERVAL // seconds;
#define ROUTE_PURGE_FREQUENCY		2 // seconds


#define ROUTE_FRESH		0x01
#define ROUTE_EXPIRED		0x02
#define ROUTE_FAILED		0x03

class INSENS;
int COUNT=0;
FILE *f;

// ======================================================================
//  Timers : Beacon Timer, Route Cache Timer
// ======================================================================

class insensBeaconTimer : public Handler {
public:
    insensBeaconTimer(INSENS* a) : agent(a) {}
    void	handle(Event*);
private:
    INSENS    *agent;
    Event	intr;
};

class insensRouteCacheTimer : public Handler {
public:
    insensRouteCacheTimer(INSENS* a) : agent(a) {}
    void	handle(Event*);
private:
    INSENS  *agent;
    Event	intr;
};

class insensFeedbackTimer : public Handler {
public:
    insensFeedbackTimer(INSENS* a) : agent(a) {}
    void	handle(Event*);
private:
    INSENS    *agent;
    Event	intr;
};

// ======================================================================
//  Route Cache Table
// ======================================================================

class RouteCache {
    friend class INSENS;
public:
    RouteCache(nsaddr_t bsrc, u_int32_t bid) {
    	rt_dst = bsrc;
    	rt_seqno = bid;
    }
protected:
    LIST_ENTRY(RouteCache) rt_link;
    u_int32_t       rt_seqno;	// route sequence number
    nsaddr_t        rt_dst;		// route destination
    nsaddr_t	rt_nexthop;	// next hop node towards the destionation
    //u_int32_t	rt_xpos;	// x position of destination;
    //u_int32_t	rt_ypos;	// y position of destination;
    u_int8_t	rt_state;	// state of the route: FRESH, EXPIRED, FAILED (BROKEN)
    u_int8_t	rt_hopcount;    // number of hops up to the destination (sink)
    double          rt_expire; 	// when route expires : Now + DEFAULT_ROUTE_EXPIRE

};
LIST_HEAD(insens_rtcache, RouteCache);


// ======================================================================
//  INSENS Routing Agent : the routing protocol
// ======================================================================

class INSENS : public Agent {
    friend class RouteCacheTimer;

public:
    INSENS(nsaddr_t id);

    
    
    void	recv(Packet *p, Handler *);
    int		command(int, const char *const *);

    // Agent Attributes
    u_int32_t   neighbor_id;
    u_int16_t   ows;       // nodes ows
    nsaddr_t	index;     // node address (identifier)
    nsaddr_t	seqno;     // beacon sequence number (used only when agent is sink)
    
    nsaddr_t    prev_hop;
    nbr_info    *nbr;
    
    u_int32_t   prev_feedback;
    nsaddr_t    prev_feedback_src;
    
    int         arr[100][100];
    
    nsaddr_t    head;

    // Node Location
    uint32_t	posx;       // position x;
    uint32_t	posy;       // position y;


    // Routing Table Management
    void		rt_insert(nsaddr_t src, u_int32_t id, nsaddr_t nexthop, u_int8_t hopcount);
    void		rt_remove(RouteCache *rt);
    void		rt_purge();
    RouteCache*	rt_lookup(nsaddr_t dst);

    // Timers
    insensBeaconTimer		bcnTimer;
    insensRouteCacheTimer	rtcTimer;
    insensFeedbackTimer	feedTimer;
    
    // Caching Head
    insens_rtcache	rthead;
    
    // Send Routines
    void		send_request();
    void		send_feedback();
    void		send_update(nsaddr_t dst);
    void		send_error(nsaddr_t unreachable_destination);
    void		forward(Packet *p, nsaddr_t nexthop, double delay);

    // Recv Routines
    void		recv_data(Packet *p);
    void		recv_insens(Packet *p);
    void 		recv_request(Packet *p);
    void		recv_feedback(Packet *p);
    void		recv_update(Packet *p);


    // Position Management
    void		update_position();

    //  A mechanism for logging the contents of the routing table.
    Trace		*logtarget;

    // A pointer to the network interface queue that sits between the "classifier" and the "link layer"
    PriQueue	*ifqueue;

    // Port classifier for passing packets up to agents
    PortClassifier	*dmux_;

};


#endif /* __insens_h__ */
