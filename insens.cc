#include <insens/insens.h>
#include <insens/insens_packet.h>
#include <random.h>
#include <cmu-trace.h>
#include <energy-model.h>
#include <tclcl.h>
#include <mobilenode.h>
#include <cmath>

#define max(a,b)        ( (a) > (b) ? (a) : (b) )
#define CURRENT_TIME    Scheduler::instance().clock()

#define DEBUG

// ======================================================================
//  TCL Hooking Classes
// ======================================================================

int hdr_insens::offset_;
static class INSENSHeaderClass : public PacketHeaderClass {
public:
    INSENSHeaderClass() : PacketHeaderClass("PacketHeader/INSENS", sizeof(hdr_all_insens)) {
        bind_offset(&hdr_insens::offset_);
    }
} class_rtProtoINSENS_hdr;

static class INSENSclass : public TclClass {
public:
    INSENSclass() : TclClass("Agent/INSENS") {}
    TclObject* create(int argc, const char*const* argv) {
        assert(argc == 5);
        return (new INSENS((nsaddr_t) Address::instance().str2addr(argv[4])));
    }
} class_rtProtoINSENS;


int
INSENS::command(int argc, const char*const* argv) {
    if(argc == 2) {
        Tcl& tcl = Tcl::instance();

        if(strncasecmp(argv[1], "id", 2) == 0) {
            tcl.resultf("%d", index);
            return TCL_OK;
        }

        if(strncasecmp(argv[1], "start", 5) == 0) {
            rtcTimer.handle((Event*) 0);
            return TCL_OK;
        }

        // Start Beacon Timer (which sends beacon message)
        if(strncasecmp(argv[1], "sink", 4) == 0) {
            bcnTimer.handle((Event*) 0);
//#ifdef DEBUG
            printf( "N (%.6f): sink node is set to %d, start request  \n", CURRENT_TIME, index);
//endif
            return TCL_OK;
        }
        if(strncasecmp(argv[1], "feedback", 8) == 0) {
            feedTimer.handle((Event*) 0);
            //#ifdef DEBUG
            printf( "N (%.6f): feed is set to %d, start feedback \n", CURRENT_TIME, index);
            //endif
            return TCL_OK;
        }
    }
    else if(argc == 3) {
        if(strcmp(argv[1], "index") == 0) {
            index = atoi(argv[2]);
            return TCL_OK;
        }

        else if(strcmp(argv[1], "log-target") == 0 || strcmp(argv[1], "tracetarget") == 0) {
            logtarget = (Trace*) TclObject::lookup(argv[2]);
            if(logtarget == 0)
                return TCL_ERROR;
            return TCL_OK;
        }

        else if(strcmp(argv[1], "drop-target") == 0) {
            /* int stat = rqueue.command(argc,argv);
             if (stat != TCL_OK)
             return stat;
             return Agent::command(argc, argv);*/
            return TCL_OK;
        }

        else if(strcmp(argv[1], "if-queue") == 0) {
            ifqueue = (PriQueue*) TclObject::lookup(argv[2]);

            if(ifqueue == 0)
                return TCL_ERROR;
            return TCL_OK;
        }

        else if (strcmp(argv[1], "port-dmux") == 0) {
            dmux_ = (PortClassifier *)TclObject::lookup(argv[2]);
            if (dmux_ == 0) {
                fprintf (stderr, "%s: %s lookup of %s failed\n", __FILE__,
                         argv[1], argv[2]);
                return TCL_ERROR;
            }
            return TCL_OK;
        }
    }

    return Agent::command(argc, argv);
}

// ======================================================================
//  Agent Constructor
// ======================================================================

INSENS::INSENS(nsaddr_t id) : Agent(PT_INSENS), bcnTimer(this), rtcTimer(this), feedTimer(this) {
   
//#ifdef DEBUG

//endif
    ows = 1;
    index = id;
    seqno = 1;
    //if(id == 0) {
        f = fopen("without_leashes.txt", "w");

      //  for(int i = 0; i<100; i++) {
        //    for(int j = 0; j<100; j++) {
          //      arr[i][j] = 0;
           // }
       // }
   // }
    //if(id == 0)
      //  head = index;
    
    LIST_INIT(&rthead);

    MobileNode *iNode = (MobileNode *) (Node::get_node_by_address(index));

    posx = iNode->X();
    posy = iNode->Y();

    printf( "N (%.6f): Routing agent is initialized for node %d position: X %d Y %d \n", CURRENT_TIME, id, iNode->X(), iNode->X());
    logtarget = 0;
    ifqueue = 0;

}

// ======================================================================
//  Timer Functions
// ======================================================================

void
insensRouteCacheTimer::handle(Event*) {
    agent->rt_purge();
    Scheduler::instance().schedule(this, &intr, ROUTE_PURGE_FREQUENCY);
}

void
insensBeaconTimer::handle(Event*) {
    agent->send_request();
    Scheduler::instance().schedule(this, &intr, DEFAULT_BEACON_INTERVAL);
}

void
insensFeedbackTimer::handle(Event*) {
    agent->send_feedback();
    Scheduler::instance().schedule(this, &intr, DEFAULT_BEACON_INTERVAL);
}

// ======================================================================
//  Send Request Routine
// ======================================================================

void
INSENS::send_request() {
	Packet *p = Packet::alloc();
	struct hdr_cmn *ch = HDR_CMN(p);
	struct hdr_ip *ih = HDR_IP(p);
	struct hdr_insens_request *req = HDR_INSENS_REQUEST(p);

	update_position();
	// Write Channel Header
	ch->ptype() = PT_INSENS;
	ch->size() = IP_HDR_LEN + req->size();
	ch->addr_type() = NS_AF_NONE;
	ch->prev_hop_ = index;

	// Write IP Header
	ih->saddr() = index;
	ih->daddr() = IP_BROADCAST;
	ih->sport() = RT_PORT;
	ih->dport() = RT_PORT;
	ih->ttl_ = NETWORK_DIAMETER;

	// Write Request Header
	req->pkt_type = INSENS_REQUEST;
    req->timestart = CURRENT_TIME;
    req->pkt_ows = 1;
    req->beacon_id = seqno;
    //req->beacon_src = index;
    
	// update the node position before putting it in the packet
	//update_position();

	req->posx = posx;
	req->posy = posy;
    
    req->timestamp = CURRENT_TIME;

	// increase sequence number for next beacon
	seqno += 1;
    
    req->delay = 0.0;

	printf( "S (%.9f): send request by %d  \n", CURRENT_TIME, index);

	Scheduler::instance().schedule(target_, p, req->delay);
}

// ======================================================================
//  Send Feedback
// ======================================================================

void
INSENS::send_feedback() {
    Packet *p = Packet::alloc();
    struct hdr_cmn *ch = HDR_CMN(p);
    struct hdr_ip *ih = HDR_IP(p);
    struct hdr_insens_feedback *feed = HDR_INSENS_FEEDBACK(p);
    
    // Write Channel Header
    ch->ptype() = PT_INSENS;
    ch->size() = IP_HDR_LEN + feed->size();
    ch->addr_type() = NS_AF_INET;
    ch->next_hop_ = prev_hop;
    ch->prev_hop_ = index;
    
    // Write IP Header
    ih->saddr() = index;
    ih->daddr() = 0;
    ih->sport() = RT_PORT;
    ih->dport() = RT_PORT;
    ih->ttl_ = NETWORK_DIAMETER;
    
    // Write Request Header
    feed->pkt_type = INSENS_FEEDBACK;
    //req->pkt_ows = 1;
    feed->timestart = CURRENT_TIME;
    feed->beacon_id = seqno;
    feed->prev_hop = prev_hop;
    //feed->beacon_src = index;
    
    //nbr_info *tmp = new nbr_info;
    //feed->nbr = new nbr_info;
    //tmp->hopid = prev_hop;
    //tmp->next = nbr;
    feed->nbr = new nbr_info;
    feed->nbr = nbr;
    //fclose(f);
    //feed->prev_hop = prev_hop;
    
    // update the node position before putting it in the packet
    //update_position();
    
    feed->posx = posx;
    feed->posy = posy;
    
    feed->timestamp = CURRENT_TIME;
    
    // increase sequence number for next beacon
    seqno += 1;

    printf( "S (%.6f): send feedback by %d  \n", CURRENT_TIME, index);

    feed->delay = 0.1 + Random::uniform();

    Scheduler::instance().schedule(target_, p, feed->delay);
}

// ======================================================================
//  Send Update
// ======================================================================

void
INSENS::send_update(nsaddr_t dst) {
    
    Packet *p = Packet::alloc();
    struct hdr_cmn *ch = HDR_CMN(p);
    struct hdr_ip *ih = HDR_IP(p);
    struct hdr_insens_update *upd = HDR_INSENS_UPDATE(p);
    
    // Write Channel Header
    ch->ptype() = PT_INSENS;
    ch->size() = IP_HDR_LEN + upd->size();
    ch->addr_type() = NS_AF_INET;
    ch->prev_hop_ = index;
    
    // Write IP Header
    ih->saddr() = index;
    ih->daddr() = dst;
    ih->sport() = RT_PORT;
    ih->dport() = RT_PORT;
    ih->ttl_ = NETWORK_DIAMETER;
    
    // Write Request Header
    upd->pkt_type = INSENS_UPDATE;
   
    // update the node position before putting it in the packet
    //update_position();
    
    //req->posx = posx;
    //req->posy = posy;
    
    upd->timestamp = CURRENT_TIME;
    
    // increase sequence number for next beacon
    seqno += 1;
    
    printf( "S (%.6f): send feedback by %d  \n", CURRENT_TIME, index);
    
    Scheduler::instance().schedule(target_, p, 0.1 + Random::uniform());
}


// ======================================================================
//  Forward Routine
// ======================================================================

void
INSENS::forward(Packet *p, nsaddr_t nexthop, double delay) {
    struct hdr_cmn *ch = HDR_CMN(p);
    struct hdr_ip *ih = HDR_IP(p);

    if (ih->ttl_ == 0) {
        drop(p, DROP_RTR_TTL);
    }
    
    if (nexthop != (nsaddr_t) IP_BROADCAST) {
        ch->next_hop_ = nexthop;
        ch->prev_hop_ = index;
        ch->addr_type() = NS_AF_INET;
        ch->direction() = hdr_cmn::DOWN;
    } else if (index >= 100){
        ch->addr_type() = NS_AF_NONE;
        ch->direction() = hdr_cmn::DOWN;
    } else {
        assert(ih->daddr() == (nsaddr_t) IP_BROADCAST);
        ch->prev_hop_ = index;
        ch->addr_type() = NS_AF_NONE;
        ch->direction() = hdr_cmn::DOWN;
    }
    
    //printf( "S (%.9f): forward by %d  \n", CURRENT_TIME, index);

    Scheduler::instance().schedule(target_, p, delay);

}

// ======================================================================
//  Recv Packet
// ======================================================================

void
INSENS::recv(Packet *p, Handler*) {
    struct hdr_cmn *ch = HDR_CMN(p);
    struct hdr_ip *ih = HDR_IP(p);

    // if the packet is routing protocol control packet, give the packet to agent

    if(ch->ptype() == PT_INSENS) {
        ih->ttl_ -= 1;
        recv_insens(p);
        return;
    }

    //  Must be a packet I'm originating
    if((ih->saddr() == index) && (ch->num_forwards() == 0)) {

        // Add the IP Header. TCP adds the IP header too, so to avoid setting it twice,
        // we check if  this packet is not a TCP or ACK segment.

        if (ch->ptype() != PT_TCP && ch->ptype() != PT_ACK) {
            ch->size() += IP_HDR_LEN;
        }

    }

    // I received a packet that I sent.  Probably routing loop.
    else if(ih->saddr() == index) {
        drop(p, DROP_RTR_ROUTE_LOOP);
        return;
    }

    //  Packet I'm forwarding...
    else {
        if(--ih->ttl_ == 0) {
            drop(p, DROP_RTR_TTL);
            return;
        }
    }

    // This is data packet, find route and forward packet
    recv_data(p);
}


// ======================================================================
//  Recv Data Packet
// ======================================================================

void
INSENS::recv_data(Packet *p) {
    struct hdr_ip *ih = HDR_IP(p);
    RouteCache *rt;

    // if route fails at link layer, (link layer could not find next hop node) it will cal rt_failed_callback function
    //ch->xmit_failure_ = rt_failed_callback;
    //ch->xmit_failure_data_ = (void*) this;

//#ifdef DEBUG
    printf( "R (%.6f): recv data by %d  \n", CURRENT_TIME, index);
//endif

    rt = rt_lookup(ih->daddr());

    // There is no route for the destination
    if (rt == NULL) {
        // TODO: queue the packet and wait for the route construction
        return ;
    }

    // if the route is not failed forward it;
    else if (rt->rt_state != ROUTE_FAILED) {
        forward(p, rt->rt_nexthop, 0.0);
    }

    // if the route has failed, wait to be updated;
    else {
        //TODO: queue the packet and wait for the route construction;
        return;
    }

}

// ======================================================================
//  Recv INSENS Packet
// ======================================================================

void
INSENS::recv_insens(Packet *p) {
    struct hdr_insens *ins = HDR_INSENS(p);

    assert(HDR_IP (p)->sport() == RT_PORT);
    assert(HDR_IP (p)->dport() == RT_PORT);
    
    switch(ins->pkt_type) {

        case INSENS_REQUEST:
            recv_request(p);
            break;

        case INSENS_FEEDBACK:
        	recv_feedback(p);
            break;

        case INSENS_UPDATE:
            recv_update(p);
            break;

        default:
            fprintf (stderr, "Invalid packet type (%x)\n", ins->pkt_type);
            exit(1);
    }
    
}

// ======================================================================
//  Recv Request Packet
// ======================================================================

void
INSENS::recv_request(Packet *p) {
    //Tcl& tcl = Tcl::instance();
    struct hdr_cmn *ch = HDR_CMN(p);
    struct hdr_ip *ih = HDR_IP(p);
    struct hdr_insens_request *req = HDR_INSENS_REQUEST(p);

    double te = (double) (req->timestart + req->delay + (double) DISTANCE / (double) VC + ACCURACY);
    
    update_position();

    //time_t te = (time_t) (req->timestart + req->delay);
    u_int32_t dsr = 0;
    if (index <= 100) {
    	dsr = abs(sqrt(req->posx*req->posx + req->posy*req->posy) - sqrt(posx*posx + posy*posy));
    	req->timestart = CURRENT_TIME;
    }

    if(dsr > DISTANCE){
    	printf( "CURRENT_TIME: (%.9f) delete packet\n",CURRENT_TIME);
        Packet::free(p);
        return;
    }
    
    /*f(te <= CURRENT_TIME) {
    	printf( "CURRENT_TIME: (%.9f) TE: (%.9f)---------------\n",CURRENT_TIME, te);
        Packet::free(p);
        return;
    }*/

    if(/*ih->saddr() <= 100 && ih->saddr() >= 0 &&*/ ch->prev_hop_ != index){
        if(nbr == NULL){
            nbr = new nbr_info;
            nbr->hopid = ch->prev_hop_;
            nbr->next = NULL;
        } else {
            nbr_info *temp = new nbr_info;
            temp->hopid = ch->prev_hop_;
            temp->next = nbr;
            nbr = temp;
            delete temp;
        }

    }
    // I have originated the packet, just drop i
    
    if (index == 0) {
        Packet::free(p);
        return;
    }
    
    if(req->pkt_ows != ows) {
        Packet::free(p);
        return;
    }
    
    printf( "R (%.6f): recv request by %d, te: %.9f, src: %d, prev: %d, seqno: %d, hop: %d, ows: %d \n",
           CURRENT_TIME, index, ih->saddr(), CURRENT_TIME/*te*/, ch->prev_hop_, req->beacon_id, req->beacon_hops ,ows);
    fprintf(f, "%.9f\n", CURRENT_TIME);
    prev_hop = ch->prev_hop_;
    ows = 0;
    req->beacon_hops +=1; // increase hop count

    req->delay = 0.1 + Random::uniform();

    printf( "F (%.9f): delay: %.9f, NEW ROUTE, forward request by %d \n", CURRENT_TIME, req->delay, index);
    
    forward(p, IP_BROADCAST, req->delay);
    //tcl.eval("puts forwarding request");

    // if the route is shorter than I have, update it
/*    if ((req->beacon_id == rt->rt_seqno) && (req->beacon_hops < rt->rt_hopcount )) {

        rt->rt_seqno = req->beacon_id;
        rt->rt_nexthop = ih->saddr();
        //rt->rt_xpos = req->beacon_posx;
        //rt->rt_ypos = req->beacon_posy;
        rt->rt_state = ROUTE_FRESH;
        rt->rt_hopcount = req->beacon_hops;
        rt->rt_expire = CURRENT_TIME + DEFAULT_ROUTE_EXPIRE;
    }*/

}

// ======================================================================
//  Recv Feedback Packet
// ======================================================================

void
INSENS::recv_feedback(Packet *p) {
    //Tcl& tcl = Tcl::instance();
    struct hdr_cmn *ch = HDR_CMN(p);
    struct hdr_ip *ih = HDR_IP(p);
    struct hdr_insens_feedback *feed = HDR_INSENS_FEEDBACK(p);
    
    double te = (double) (feed->timestart + feed->delay + (double) DISTANCE / (double) VC + ACCURACY);
   
    // u_int32_t dsr = + ACCURACY;
    //
    //if(dsr > DISTANCE){
      //  Packet::free(p);
        //return;
    //}

    
    if(te <= CURRENT_TIME){
    	Packet::free(p);
        return;
    }
    
    if (ch->next_hop_ != index) {
        Packet:free(p);
        return;
    }
    //printf( "I (%.6f): index: %d, saddr: %d \n", CURRENT_TIME,index, ih->saddr());
    printf( "R (%.6f): recv feedback by %d, src: %d, seqno:%d, hop: %d \n",
           CURRENT_TIME, index, ih->saddr(), feed->beacon_id, feed->beacon_hops);

    if (index == ih->daddr())  {
        nbr_info *tmp = feed->nbr;
        while(tmp->next != NULL){
            printf("%d - ", tmp->hopid);
         //   if(tmp->hopid < 100 && tmp->hopid >= 0)
           //     arr[ih->saddr()][tmp->hopid] = 1;
            tmp = tmp->next;
        }
        printf("\n");

        Packet::free(p);
        return;
    }

    //fprintf(f, "%.9f\n", CURRENT_TIME);

    //feed->prev_hop = prev_hop;
    //ih->saddr() = index;
    feed->beacon_hops +=1; // increase hop count
    double delay = 0.1 + Random::uniform();

    feed->delay = delay;
    
    printf( "F (%.6f): forward feedback by %d \n", CURRENT_TIME, index);

    forward(p, prev_hop, delay);

}


// ======================================================================
//  Recv Update Packet
// ======================================================================

void
INSENS::recv_update(Packet *p) {
}

// ======================================================================
//  Routing Management
// ======================================================================

void
INSENS::rt_insert(nsaddr_t src, u_int32_t id, nsaddr_t nexthop, u_int8_t hopcount) {
    RouteCache	*rt = new RouteCache(src, id);

    rt->rt_nexthop = nexthop;
    //rt->rt_xpos = xpos;
    //rt->rt_ypos = ypos;
    rt->rt_state = ROUTE_FRESH;
    rt->rt_hopcount = hopcount;
    rt->rt_expire = CURRENT_TIME + DEFAULT_ROUTE_EXPIRE;

    LIST_INSERT_HEAD(&rthead, rt, rt_link);
}

RouteCache*
INSENS::rt_lookup(nsaddr_t dst) {
    RouteCache *r = rthead.lh_first;

    for( ; r; r = r->rt_link.le_next) {
        if (r->rt_dst == dst)
            return r;
    }

    return NULL;
}

void
INSENS::rt_purge() {
    RouteCache *rt= rthead.lh_first;
    double now = CURRENT_TIME;

    for(; rt; rt = rt->rt_link.le_next) {
        if(rt->rt_expire <= now)
            rt->rt_state = ROUTE_EXPIRED;
    }
}

void
INSENS::rt_remove(RouteCache *rt) {
    LIST_REMOVE(rt,rt_link);
}


void
INSENS::update_position() {
    //TODO: we have to update node position
	 MobileNode *iNode = (MobileNode *) (Node::get_node_by_address(index));

	 posx = iNode->X();
	 posy = iNode->Y();

	 //printf( "UPDATE POSITION (%.6f): X %d Y %d \n", CURRENT_TIME, posx, posy);

}
