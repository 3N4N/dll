#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ******************************************************************
 ALTERNATING BIT AND GO-BACK-N NETWORK EMULATOR: SLIGHTLY MODIFIED
 FROM VERSION 1.1 of J.F.Kurose

   This code should be used for PA2, unidirectional or bidirectional
   data transfer protocols (from A to B. Bidirectional transfer of data
   is for extra credit and is not required).  Network properties:
   - one way network delay averages five time units (longer if there
       are other packets in the channel for GBN), but can be larger
   - frames can be corrupted (either the header or the data portion)
       or lost, according to user-defined probabilities
   - frames will be delivered in the order in which they were sent
       (although some can be lost).
**********************************************************************/

#define BIDIRECTIONAL 1 /* change to 1 if you're doing extra credit */
/* and write a routine called B_output */

/* a "pkt" is the data unit passed from layer 3 (teachers code) to layer  */
/* 2 (students' code).  It contains the data (characters) to be delivered */
/* to layer 3 via the students transport level protocol entities.         */
struct pkt
{
    char data[20];
};

enum frmtype {
    DATA,
    ACK,
    PACK
};

/* a frame is the data unit passed from layer 2 (students code) to layer */
/* 1 (teachers code).  Note the pre-defined frame structure, which all   */
/* students must follow. */
struct frm
{
    enum frmtype type;
    int seqnum;
    int acknum;
    int checksum;
    char payload[20];
};

/********* FUNCTION PROTOTYPES. DEFINED IN THE LATER PART******************/
void starttimer(int AorB, float increment);
void stoptimer(int AorB);
void tolayer1(int AorB, struct frm frame);
void tolayer3(int AorB, char datasent[20]);

/********* STUDENTS WRITE THE NEXT SEVEN ROUTINES *********/

enum State {
    WAITING_FOR_LAYER3,
    WAITING_FOR_ACK
};

struct Entity {
    enum State state;
    int seq;
    float timerInterrupt;
    struct frm lastFrame;
}A, B;

int inc_seq(int seq) {
    /* Since the sequence is alternating
     * it can have only two values: 0 and 1. */
    return 1 - seq;
}

/*
 * Returns the summation of the int values
 * of all members
 */
int get_checksum(struct frm *frame) {
    int checksum = 0;
    checksum += frame->seqnum;
    checksum += frame->acknum;

    int i;
    for (i = 0; i < 20; ++i)
        checksum += frame->payload[i];

    return checksum;
}

/* called from layer 3, passed the data to be sent to other side */
void A_output(struct pkt packet)
{
    if (A.state != WAITING_FOR_LAYER3) {
        printf("  A_output: Packet dropped. ACK not yet received.\n");
        return;
    }

    /* create a frame to send B */
    struct frm frame;
    frame.type = DATA;
    frame.seqnum = A.seq;
    frame.acknum = -1;
    memmove(frame.payload, packet.data, 20);
    frame.checksum = get_checksum(&frame);

    /* send the frame to B */
    A.lastFrame = frame;
    A.state = WAITING_FOR_ACK;
    tolayer1(0, frame);
    starttimer(0, A.timerInterrupt);

    printf("  A_output: Frame sent: %s.\n", packet.data);
}

/* need be completed only for extra credit */
void B_output(struct pkt packet)
{
    if (B.state != WAITING_FOR_LAYER3) {
        printf("  B_output: Packet dropped. ACK not yet received.\n");
        return;
    }

    /* create a frame to send B */
    struct frm frame;
    frame.type = DATA;
    frame.seqnum = B.seq;
    frame.acknum = -1;
    memmove(frame.payload, packet.data, 20);
    frame.checksum = get_checksum(&frame);

    /* send the frame to B */
    B.lastFrame = frame;
    B.state = WAITING_FOR_ACK;
    tolayer1(1, frame);
    starttimer(1, B.timerInterrupt);

    printf("  B_output: Frame sent: %s.\n", packet.data);
}

/* called from layer 1, when a frame arrives for layer 2 */
void A_input(struct frm frame)
{
    if (frame.type == ACK) {
        if (A.state != WAITING_FOR_ACK) {
            printf("  A_input: Frame dropped. Unidirectional only.\n");
            return;
        }

        if (frame.checksum != get_checksum(&frame)) {
            printf("  A_input: ACK dropped. Corruption.\n");
            return;
        }

        if (frame.acknum != A.seq) {
            printf("  A_input: ACK dropped. Not the expected ACK.\n");
            return;
        }

        printf("  A_input: ACK received.\n");

        /* get ready for sendig next packet */
        stoptimer(0);
        A.seq = inc_seq(A.seq);
        A.state = WAITING_FOR_LAYER3;
    }
    else if (frame.type == DATA) {
        if (frame.checksum != get_checksum(&frame)) {
            printf("  A_input: Frame corrupted. Send NACK.\n");
            send_ack(0, inc_seq(A.seq));
            return;
        }

        if (frame.seqnum != A.seq) {
            printf("  A_input: Not the expected SEQ. Send NACK.\n");
            send_ack(0, inc_seq(A.seq));
            return;
        }

        printf("  A_input: Frame received: %s\n", frame.payload);

        printf("  A_input: Send ACK.\n");
        send_ack(0, A.seq);

        tolayer3(0, frame.payload);
        A.seq = inc_seq(A.seq);
    }
}

/* called when A's timer goes off */
void A_timerinterrupt(void)
{
    if (A.state != WAITING_FOR_ACK) {
        printf("  A_timerinterrupt: Ignored. Not waiting for ACK.\n");
        return;
    }

    printf("  A_timerinterrupt: Resend last frame: %s.\n", A.lastFrame.payload);
    tolayer1(0, A.lastFrame);
    starttimer(0, A.timerInterrupt);
}

/* the following routine will be called once (only) before any other */
/* entity A routines are called. You can use it to do any initialization */
void A_init(void)
{
    A.state = WAITING_FOR_LAYER3;
    A.seq = 0;
    A.timerInterrupt = 25;
}

void send_ack(int AorB, int ack)
{
    struct frm frame;
    frame.type = ACK;
    frame.acknum = ack;
    frame.checksum = get_checksum(&frame);
    tolayer1(AorB, frame);
}

/* Note that with simplex transfer from a-to-B, there is no B_output() */
/* called from layer 1, when a frame arrives for layer 2 at B*/
void B_input(struct frm frame)
{
    if (frame.type == ACK) {
        if (B.state != WAITING_FOR_ACK) {
            printf("  B_input: Frame dropped. Unidirectional only.\n");
            return;
        }

        if (frame.checksum != get_checksum(&frame)) {
            printf("  B_input: ACK dropped. Corruption.\n");
            return;
        }

        if (frame.acknum != B.seq) {
            printf("  B_input: ACK dropped. Not the expected ACK.\n");
            return;
        }

        printf("  B_input: ACK received.\n");

        /* get ready for sendig next packet */
        stoptimer(1);
        B.seq = inc_seq(B.seq);
        B.state = WAITING_FOR_LAYER3;
    }
    else if (frame.type == DATA) {
        if (frame.checksum != get_checksum(&frame)) {
            printf("  B_input: Frame corrupted. Send NACK.\n");
            send_ack(1, inc_seq(B.seq));
            return;
        }

        if (frame.seqnum != B.seq) {
            printf("  B_input: Not the expected SEQ. Send NACK.\n");
            send_ack(1, inc_seq(B.seq));
            return;
        }

        printf("  B_input: Frame received: %s\n", frame.payload);

        printf("  B_input: Send ACK.\n");
        send_ack(1, B.seq);

        tolayer3(1, frame.payload);
        B.seq = inc_seq(B.seq);
    }
}

/* called when B's timer goes off */
void B_timerinterrupt(void)
{
    if (B.state != WAITING_FOR_ACK) {
        printf("  B_timerinterrupt: Ignored. Not waiting for ACK.\n");
        return;
    }

    printf("  B_timerinterrupt: Resend last frame: %s.\n", B.lastFrame.payload);
    tolayer1(1, B.lastFrame);
    starttimer(1, B.timerInterrupt);
}

/* the following rouytine will be called once (only) before any other */
/* entity B routines are called. You can use it to do any initialization */
void B_init(void)
{
    B.state = WAITING_FOR_ACK;
    B.seq = 0;
    B.timerInterrupt = 25;
}


/*****************************************************************
***************** NETWORK EMULATION CODE STARTS BELOW ***********
The code below emulates the layer 1 and below network environment:
    - emulates the tranmission and delivery (possibly with bit-level corruption
        and frame loss) of frames across the layer 1/2 interface
    - handles the starting/stopping of a timer, and generates timer
        interrupts (resulting in calling students timer handler).
    - generates packet to be sent (passed from later 3 to 2)

THERE IS NOT REASON THAT ANY STUDENT SHOULD HAVE TO READ OR UNDERSTAND
THE CODE BELOW.  YOU SHOLD NOT TOUCH, OR REFERENCE (in your code) ANY
OF THE DATA STRUCTURES BELOW.  If you're interested in how I designed
the emulator, you're welcome to look at the code - but again, you should have
to, and you defeinitely should not have to modify
******************************************************************/

struct event
{
    float evtime;       /* event time */
    int evtype;         /* event type code */
    int eventity;       /* entity where event occurs */
    struct frm *frmptr; /* ptr to frame (if any) assoc w/ this event */
    struct event *prev;
    struct event *next;
};
struct event *evlist = NULL; /* the event list */

/* possible events: */
#define TIMER_INTERRUPT 0
#define FROM_LAYER3 1
#define FROM_LAYER1 2

#define OFF 0
#define ON 1
#define A 0
#define B 1

int TRACE = 1;     /* for my debugging */
int nsim = 0;      /* number of packets from 3 to 2 so far */
int nsimmax = 0;   /* number of pkts to generate, then stop */
float time = 0.000;
float lossprob;    /* probability that a frame is dropped  */
float corruptprob; /* probability that one bit is frame is flipped */
float lambda;      /* arrival rate of packets from layer 3 */
int ntolayer1;     /* number sent into layer 1 */
int nlost;         /* number lost in media */
int ncorrupt;      /* number corrupted by media*/

void init();
void generate_next_arrival(void);
void insertevent(struct event *p);

#define WRITE_DOC 1

FILE *fp;

int main()
{
    struct event *eventptr;
    struct pkt pkt2give;
    struct frm frm2give;

    int i, j;
    char c;

    init();
    A_init();
    B_init();

    while (1)
    {
        eventptr = evlist; /* get next event to simulate */
        if (eventptr == NULL)
            goto terminate;
        evlist = evlist->next; /* remove this event from event list */
        if (evlist != NULL)
            evlist->prev = NULL;
        if (TRACE >= 2)
        {
            printf("\nEVENT time: %f,", eventptr->evtime);
            printf("  type: %d", eventptr->evtype);
            if (eventptr->evtype == 0)
                printf(", timerinterrupt  ");
            else if (eventptr->evtype == 1)
                printf(", fromlayer3 ");
            else
                printf(", fromlayer1 ");
            printf(" entity: %d\n", eventptr->eventity);
        }
        time = eventptr->evtime; /* update time to next event time */
        if (eventptr->evtype == FROM_LAYER3)
        {
            if (nsim < nsimmax)
            {
                if (nsim + 1 < nsimmax)
                    generate_next_arrival(); /* set up future arrival */
                /* fill in pkt to give with string of same letter */
                j = nsim % 26;
                for (i = 0; i < 20; i++)
                    pkt2give.data[i] = 97 + j;
                pkt2give.data[19] = 0;
                if (TRACE > 2)
                {
                    printf("          MAINLOOP: data given to student: ");
                    for (i = 0; i < 20; i++)
                        printf("%c", pkt2give.data[i]);
                    printf("\n");
                }
                nsim++;
                if (eventptr->eventity == A)
                    A_output(pkt2give);
                else
                    B_output(pkt2give);
            }
        }
        else if (eventptr->evtype == FROM_LAYER1)
        {
            frm2give.type = eventptr->frmptr->type;
            frm2give.seqnum = eventptr->frmptr->seqnum;
            frm2give.acknum = eventptr->frmptr->acknum;
            frm2give.checksum = eventptr->frmptr->checksum;
            for (i = 0; i < 20; i++)
                frm2give.payload[i] = eventptr->frmptr->payload[i];
            if (eventptr->eventity == A) /* deliver frame by calling */
                A_input(frm2give); /* appropriate entity */
            else
                B_input(frm2give);
            free(eventptr->frmptr); /* free the memory for frame */
        }
        else if (eventptr->evtype == TIMER_INTERRUPT)
        {
            if (eventptr->eventity == A)
                A_timerinterrupt();
            else
                B_timerinterrupt();
        }
        else
        {
            printf("INTERNAL PANIC: unknown event type \n");
        }
        free(eventptr);
    }

terminate:
    printf(
        " Simulator terminated at time %f\n after sending %d pkts from layer3\n",
        time, nsim);

    if (WRITE_DOC == 1) fclose(fp);
}

void init() /* initialize the simulator */
{
    int i;
    float sum, avg;
    float jimsrand();

    printf("-----  Stop and Wait Network Simulator Version 1.1 -------- \n\n");

    // printf("Enter the number of packets to simulate: ");
    // scanf("%d",&nsimmax);
    // printf("Enter  frame loss probability [enter 0.0 for no loss]:");
    // scanf("%f",&lossprob);
    // printf("Enter frame corruption probability [0.0 for no corruption]:");
    // scanf("%f",&corruptprob);
    // printf("Enter average time between packets from sender's layer3 [ > 0.0]:");
    // scanf("%f",&lambda);
    // printf("Enter TRACE:");
    // scanf("%d",&TRACE);

    nsimmax     = 2;
    lossprob    = 0.0;
    corruptprob = 0.2;
    lambda      = 1000;
    TRACE       = 0;

    if (WRITE_DOC == 1) fp = freopen("report.txt", "w+", stdout);

    // printf("\n\n");
    // printf("The number of packets to simulate: %d\n", nsimmax);
    // printf("Frame loss probability: %f\n", lossprob);
    // printf("Frame corruption probability: %f\n", corruptprob);
    // printf("Average time between packets from sender's layer3: %f\n", lambda);
    // printf("TRACE: %d\n", TRACE);

    srand(9999); /* init random number generator */
    sum = 0.0;   /* test random number generator for students */
    for (i = 0; i < 1000; i++)
        sum = sum + jimsrand(); /* jimsrand() should be uniform in [0,1] */
    avg = sum / 1000.0;
    if (avg < 0.25 || avg > 0.75)
    {
        printf("It is likely that random number generation on your machine\n");
        printf("is different from what this emulator expects.  Please take\n");
        printf("a look at the routine jimsrand() in the emulator code. Sorry. \n");
        exit(1);
    }

    ntolayer1 = 0;
    nlost = 0;
    ncorrupt = 0;

    time = 0.0;              /* initialize time to 0.0 */
    generate_next_arrival(); /* initialize event list */
}

/****************************************************************************/
/* jimsrand(): return a float in range [0,1].  The routine below is used to */
/* isolate all random number generation in one location.  We assume that the*/
/* system-supplied rand() function return an int in therange [0,mmm]        */
/****************************************************************************/
float jimsrand(void)
{
    double mmm = RAND_MAX;
    float x;                 /* individual students may need to change mmm */
    x = rand() / mmm;        /* x should be uniform in [0,1] */
    return (x);
}

/********************* EVENT HANDLINE ROUTINES *******/
/*  The next set of routines handle the event list   */
/*****************************************************/

void generate_next_arrival(void)
{
    double x, log(), ceil();
    struct event *evptr;
    float ttime;
    int tempint;

    if (TRACE > 2)
        printf("          GENERATE NEXT ARRIVAL: creating new arrival\n");

    x = lambda * jimsrand() * 2; /* x is uniform on [0,2*lambda] */
    /* having mean of lambda        */
    evptr = (struct event *)malloc(sizeof(struct event));
    evptr->evtime = time + x;
    evptr->evtype = FROM_LAYER3;
    if (BIDIRECTIONAL && (jimsrand() > 0.5))
        evptr->eventity = B;
    else
        evptr->eventity = A;
    insertevent(evptr);
}

void insertevent(struct event *p)
{
    struct event *q, *qold;

    if (TRACE > 2)
    {
        printf("            INSERTEVENT: time is %lf\n", time);
        printf("            INSERTEVENT: future time will be %lf\n", p->evtime);
    }
    q = evlist;      /* q points to header of list in which p struct inserted */
    if (q == NULL)   /* list is empty */
    {
        evlist = p;
        p->next = NULL;
        p->prev = NULL;
    }
    else
    {
        for (qold = q; q != NULL && p->evtime > q->evtime; q = q->next)
            qold = q;
        if (q == NULL)   /* end of list */
        {
            qold->next = p;
            p->prev = qold;
            p->next = NULL;
        }
        else if (q == evlist)     /* front of list */
        {
            p->next = evlist;
            p->prev = NULL;
            p->next->prev = p;
            evlist = p;
        }
        else     /* middle of list */
        {
            p->next = q;
            p->prev = q->prev;
            q->prev->next = p;
            q->prev = p;
        }
    }
}

void printevlist(void)
{
    struct event *q;
    int i;
    printf("--------------\nEvent List Follows:\n");
    for (q = evlist; q != NULL; q = q->next)
    {
        printf("Event time: %f, type: %d entity: %d\n", q->evtime, q->evtype,
               q->eventity);
    }
    printf("--------------\n");
}

/********************** Student-callable ROUTINES ***********************/

/* called by students routine to cancel a previously-started timer */
void stoptimer(int AorB /* A or B is trying to stop timer */)
{
    struct event *q, *qold;

    if (TRACE > 2)
        printf("          STOP TIMER: stopping timer at %f\n", time);
    /* for (q=evlist; q!=NULL && q->next!=NULL; q = q->next)  */
    for (q = evlist; q != NULL; q = q->next)
        if ((q->evtype == TIMER_INTERRUPT && q->eventity == AorB))
        {
            /* remove this event */
            if (q->next == NULL && q->prev == NULL)
                evlist = NULL;          /* remove first and only event on list */
            else if (q->next == NULL) /* end of list - there is one in front */
                q->prev->next = NULL;
            else if (q == evlist)   /* front of list - there must be event after */
            {
                q->next->prev = NULL;
                evlist = q->next;
            }
            else     /* middle of list */
            {
                q->next->prev = q->prev;
                q->prev->next = q->next;
            }
            free(q);
            return;
        }
    printf("Warning: unable to cancel your timer. It wasn't running.\n");
}

void starttimer(int AorB /* A or B is trying to start timer */, float increment)
{
    struct event *q;
    struct event *evptr;

    if (TRACE > 2)
        printf("          START TIMER: starting timer at %f\n", time);
    /* be nice: check to see if timer is already started, if so, then  warn */
    /* for (q=evlist; q!=NULL && q->next!=NULL; q = q->next)  */
    for (q = evlist; q != NULL; q = q->next)
        if ((q->evtype == TIMER_INTERRUPT && q->eventity == AorB))
        {
            printf("Warning: attempt to start a timer that is already started\n");
            return;
        }

    /* create future event for when timer goes off */
    evptr = (struct event *)malloc(sizeof(struct event));
    evptr->evtime = time + increment;
    evptr->evtype = TIMER_INTERRUPT;
    evptr->eventity = AorB;
    insertevent(evptr);
}

/************************** TOLAYER1 ***************/
void tolayer1(int AorB, struct frm frame)
{
    struct frm *myfrmptr;
    struct event *evptr, *q;
    float lastime, x;
    int i;

    ntolayer1++;

    /* simulate losses: */
    if (jimsrand() < lossprob)
    {
        nlost++;
        if (TRACE > 0)
            printf("          TOLAYER1: frame being lost\n");
        return;
    }

    /* make a copy of the frame student just gave me since he/she may decide */
    /* to do something with the frame after we return back to him/her */
    myfrmptr = (struct frm *)malloc(sizeof(struct frm));
    myfrmptr->type = frame.type;
    myfrmptr->seqnum = frame.seqnum;
    myfrmptr->acknum = frame.acknum;
    myfrmptr->checksum = frame.checksum;
    for (i = 0; i < 20; i++)
        myfrmptr->payload[i] = frame.payload[i];
    if (TRACE > 2)
    {
        printf("          TOLAYER1: type : %d seq: %d, ack %d, check: %d ",
               myfrmptr->type, myfrmptr->seqnum, myfrmptr->acknum, myfrmptr->checksum);
        for (i = 0; i < 20; i++)
            printf("%c", myfrmptr->payload[i]);
        printf("\n");
    }

    /* create future event for arrival of frame at the other side */
    evptr = (struct event *)malloc(sizeof(struct event));
    evptr->evtype = FROM_LAYER1;      /* frame will pop out from layer1 */
    evptr->eventity = (AorB + 1) % 2; /* event occurs at other entity */
    evptr->frmptr = myfrmptr;         /* save ptr to my copy of frame */
    /* finally, compute the arrival time of frame at the other end.
       medium can not reorder, so make sure frame arrives between 1 and 10
       time units after the latest arrival time of frames
       currently in the medium on their way to the destination */
    lastime = time;
    /* for (q=evlist; q!=NULL && q->next!=NULL; q = q->next) */
    for (q = evlist; q != NULL; q = q->next)
        if ((q->evtype == FROM_LAYER1 && q->eventity == evptr->eventity))
            lastime = q->evtime;
    evptr->evtime = lastime + 1 + 9 * jimsrand();

    /* simulate corruption: */
    if (jimsrand() < corruptprob)
    {
        ncorrupt++;
        if ((x = jimsrand()) < .75)
            myfrmptr->payload[0] = 'Z'; /* corrupt payload */
        else if (x < .875)
            myfrmptr->seqnum = 999999;
        else
            myfrmptr->acknum = 999999;
        if (TRACE > 0)
            printf("          TOLAYER1: frame being corrupted\n");
    }

    if (TRACE > 2)
        printf("          TOLAYER1: scheduling arrival on other side\n");
    insertevent(evptr);
}

void tolayer3(int AorB, char datasent[20])
{
    int i;
    if (TRACE > 2)
    {
        printf("          TOLAYER3: data received: ");
        for (i = 0; i < 20; i++)
            printf("%c", datasent[i]);
        printf("\n");
    }
}
