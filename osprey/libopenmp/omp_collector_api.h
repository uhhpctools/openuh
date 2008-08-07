/*
 * Copyright (c) 2006, 2007, OpenMP Architecture Review Board.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */


#ifndef	_OMP_COLLECTOR_API_H
#define	_OMP_COLLECTOR_API_H


#ifdef __cplusplus
extern "C" {
#endif


/******************************************************************
 *	This file defines the communication between the OpenMP runtime
 *	library and a "collector", assumed to be a separately compiled
 *	shared object, in the address space of the target and the
 *	OpenMP runtime library
 *
 *    Rendezvous strategy:
 *	The collector asks the runtime linker for the address
 *	corresponding the the string defined as OMP_COLLECTOR_API
 *	(see #define below).  Once it has that address, it calls
 *	that function as its only API.
 *
 *    Initialization:
 *	The first request made by the collector must be a OMP_REQ_START 
 *	request.  It may be issued before or after the OpenMP runtime 
 *	.init section has been called.  If it is issued before, the runtime 
 *	library must be able to initialize itself and handle the request.
 *
 *    Threads
 *	The initial thread is created when the process is instantiated, and
 *	it is referred to as the "initial thread."
 *	Subsequent threads created by the OpenMP runtime are referred to
 *	as OpenMP threads.  Subsequent threads created by non-OpenMP APIs
 *	are referred to as "non-OpenMP threads."  The term "master thread"
 *	refers to the thread that encounters a parallel region and creates
 *	a team of threads to work on that parallel region.  The term "slave
 *	thread" refers to any thread within a team other than the
 *	master thread of that team.
 *
 *    Event notification
 *	One of the requests (OMP_REQ_REGISTER) asks for notification of 
 *	specific events (one of the enum OMP_COLLECTORAPI_EVENT values).  
 *	That callback will be made with the event enum as an argument 
 *	when each event occurs.  The first notification may occur before 
 *	the request to __omp_collector_api completes, and the collector 
 *	must be able to cope with that.
 *
 *    Statistical Profiling
 *	For statistical profiling, the collector establishes whatever
 *	trigger it wants, presumed to be a signal generated to the
 *	target.  The collector's signal handler can make calls to determine
 *	the OpenMP runtime state of the calling thread, the parallel
 *	region in which it is executing, and the lock on which it is
 *	waiting.  The state and lock id are returned from a single
 *	request; the calls are async-signal-safe.
 *
 *    Callstack reconciliation
 *	Typical OpenMP implementations do not execute code such
 *	that the callstack unwound in a thread matches the user model
 *	of execution.  User-model callstacks can be synthesized by
 *	tracing OMP_EVENT_FORK events, and recording the region ID,
 *	and the master-thread callstack at the time of that event.
 *	The callstack as unwound by a thread in that region can be
 *	appended to the master callstack on entry to that region
 *	to synthesize a user-model callstack.  One issue to be resolved
 *	is the elimination of OpenMP runtime frames from the synthesized
 *	callstack, since the runtime does not really appear in the user model.
 *	The current API provides no help in doing that synthesis.
 *
 *    Termination
 *	The collector can turn off all interactions with the runtime
 *	by sending a OMP_REQ_STOP request.  All callbacks will be 
 *	cancelled, and any subsequent requests will get a 
 *	OMP_ERRCODE_SEQUENCE_ERR response.  A new session may be 
 *	started by reissuing a OMP_REQ_START request, and a conforming
 *	implementation must support that request.
 *
 ******************************************************************/


/* Interface function in the OpenMP Runtime Library  -- string provides name */

#define OMP_COLLECTOR_API	"__omp_collector_api"


/*
 * int __omp_collector_api( void *arg );
 *
 * The interface for the OpenMP runtime library to enable OpenMP
 *	processing support for a collector, to register and unregister
 *	for event notification, and to inquire for various parameters.
 *
 * The API may be called from any thread, whether the initial thread,
 *	an OpenMP thread, or a non-OpenMP thread.  Some requests will
 *	return an error status or value if called by non-OpenMP threads.
 *
 * Returns 0 if the entire request has been successfully processed
 *	(even if some queries returned error codes),
 * otherwise -1 is returned.
 *
 * Consumer: A generic "collector" recording performance data
 * Producer: the OpenMP Runtime library
 *
 * Consumer allocates memory for 'arg' and specifies in it a request
 * for some producer's properties. Producer uses the same memory to record
 * the result of each entry in the request. The request layout is:
 *
 * arg 
 *  |
 *  v
 *  +-----+----+----+-----+-----+...+----+----+----+-----+-----+...+----+
 *  | sz  | r# | ec | rsz | mem |   | sz | r# | ec | rsz | mem |   | 0  |
 *  +-----+----+----+-----+-----+...+----+----+----+-----+-----+...+----+
 *  
 * int sz  [4 bytes] : Entry length in bytes (including the 16-byte header).
 *
 * int r#  [4 bytes] : Request number -- one of enum OMP_COLLECTOR_REQUEST.
 *
 * int ec  [4 bytes] : Error code -- one of enum OMP_COLLECTOR_EC.
 *
 * int rsz [4 bytes] : return size in bytes; if OMP_ERRCODE_MEM_TOO_SMALL
 *	is returned, rsz is the size needed for "mem" to hold the response.
 *	If no error is returned, rsz is the size of the returned data in "mem".
 *
 * char mem[]             : Input parameters and/or return value place holders.
 *
 * sz == 0 implies the end of the list of requests
 *
 * The consumer sets sz. If sz is not a multiple of 4 or less than 16, or
 *	not properly aligned the remainder of the request is ignored, and
 *	-1 is returned.
 *	The request is one of the values of OMP_COLLECTORAPI_REQUEST.
 *
 * The producer also returns a return code in the error code field 'ec'.
 *	The error code is one of the values of OMP_COLLECTORAPI_EC.
 * 
 * For known requests, the producer knows the types and sizes for the return.
 *	The size field may be greater than the actual request requires.
 *	Producer converts the pointer to the mem field to the appropriate
 *	type to record a return value. If the size of mem is smaller
 *	than required, the error code OMP_ERRCODE_TOO_SMALL is returned.
 *
 * Producer checks for alignment for robustness before recording 
 *	a property value and ignores entries if not properly aligned.
 *	In such case, producer will return OMP_ERRCODE_ERROR.
 *
 * It's the consumer's responsibility to provide correct property size and
 *	alignment. It's also the consumer's responsibility to know which
 *	requests are async-signal-safe and could be issued from a
 *	signal handler.
 *
 * The request, error code, event, and thread-state enums have explicit
 *	numerical values that will be persistent across releases.
 *
 */


/******************************************************************
 *  Request Codes
 ******************************************************************/

typedef enum {
	OMP_REQ_START = 0,
	OMP_REQ_REGISTER = 1,
	OMP_REQ_UNREGISTER = 2,
	OMP_REQ_STATE = 3,
	OMP_REQ_CURRENT_PRID = 4,
	OMP_REQ_PARENT_PRID = 5,
	OMP_REQ_STOP = 6,
	OMP_REQ_PAUSE = 7,
	OMP_REQ_RESUME = 8,
	OMP_REQ_LAST
} OMP_COLLECTORAPI_REQUEST;

/*
 * Requests may be batched in a single call; they will be processed 
 * sequentially, and will behave identically to the same requests made 
 * in independent calls.
 *	
 * Specific Requests:
 *
 *    OMP_REQ_START
 *		Must be the first request; if any other request comes
 *		prior to this one, it will get an OMP_ERRCODE_SEQUENCE_ERR 
 *		return.  The request should be issued as early as possible, 
 *		it tells the runtime to allocate and necessary space for 
 *		keeping track of thread state, region IDs, callbacks, etc.
 *		If the OMP_REQ_START arrives too late for such initialization,
 *		it will get a OMP_ERRCODE_SEQUENCE_ERR return.
 *		An OMP_REQ_START may not be issued when the API is already
 *		started -- it will get a OMP_ERRCODE_SEQUENCE_ERR return.
 *		An OMP_REQ_START may be issued after an OMP_REQ_STOP, and
 *		it is implementation-dependent as to whether it will work
 *		or return OMP_ERRCODE_SEQUENCE_ERR.
 *		OMP_REQ_START may be issued by any thread.
 *
 *	   Input parameters: none
 *	   Return Value: none
 *	   Async-signal-safe: no
 *
 *    OMP_REQ_REGISTER
 *		Passes in an event type (enum OMP_COLLECTORAPI_EVENT), and 
 *		a (void *) callback.  The callback routine will be passed in 
 *		the event type, so that a single callback routine could
 *		be defined for all events, or separate callbacks can be used.
 *		The callback function prototype is:
 *		    void	callback(enum OMP_COLLECTORAPI_EVENT);
 *		The callback will be called synchronously by the thread
 *		generating the event, not by the thread registering the callback.
 *		OMP_REQ_REGISTER may be issued by any thread.
 *
 *		If an event is not known, OMP_ERRCODE_UNKNOWN will be returned.
 *		If an event is known but not supported for callbacks,
 *		OMP_ERRCODE_UNSUPPORTED will be returned. If a 
 *		previous registration was done, the new callback replaces it.
 *		It is the responsibility of the collector, not the OpenMP
 *		runtime, to support multiple callbacks.
 *
 *	   Input parameters: {OMP_COLLECTORAPI_EVENT, void *callback}
 *	   Return Value: none
 *	   Async-signal-safe: no
 *
 *    OMP_REQ_UNREGISTER
 *		Passes in an event type.  If there was no previous registry 
 *		for that event, the request is ignored.
 *		OMP_REQ_UNREGISTER may be called by any thread
 *
 *	   Input parameters: OMP_COLLECTORAPI_EVENT
 *	   Return Value: none
 *	   Async-signal-safe: no
 *
 *    OMP_REQ_STATE
 *		Returns a value defined in the enum OMP_COLLECTOR_API_THR_STATE, below
 *		and, if applicable, a (void *) wait_id.  The wait_id is a lock
 *		ID for state THR_LKWT_STATE, THR_CTWT_STATE, and THR_ATWT_STATE;
 *		a barrier ID for states THR_IBAR_STATE and THR_EBAR_STATE; 
 *		an ordered-region ID for state THR_ODWT_STATE.
 *		It is undefined for other states.
 *	   	It may be called from any thread, and will return
 *		THR_SERIAL_STATE for the initial thread or any non-OpenMP thread that is
 *		not in a parallel region.
 *
 *	   Input parameters: none
 *	   Return Value: {OMP_COLLECTOR_API_THR_STATE, wait_id}
 *	   Async-signal-safe: yes
 *
 *    OMP_REQ_CURRENT_PRID
 *		Returns the ID of the current parallel region within which
 *		the calling thread is executing.
 *		If the calling thread is not within a parallel region either because
 *		the thread is in state THR_SERIAL_STATE or THR_IDLE_STATE,
 *		it will return 0.
 *	   	Calling thread may be any thread.
 *
 *	   Input parameters: none
 *	   Return Value: uint64_t region_id
 *	   Async-signal-safe: yes
 *
 *    OMP_REQ_PARENT_PRID
 *		Returns the ID of the parallel region in which the current patallel region
 *		is nested.  Returns 0 for an outer (non-nested) parallel region.
 *		Only meaningful when the thread is the master thread of the team.
 *		If the calling thread is an OpenMP thread, but not the master thread
 *		of a team within a parallel region, or a non-OpenMP thread,
 *	   	it will return OMP_ERRCODE_THREAD_ERR
 *		This call is normally made only from a OMP_EVENT_FORK event callback.
 *
 *	   Input parameters: none
 *	   Return Value: uint64_t region_id
 *	   Async-signal-safe: no
 *
 *    OMP_REQ_PAUSE
 *		Temporarily disables the generation of any events
 *		Ignored if events are currently already disabled
 *		PAUSE/RESUME are orthogonal to REGISTER/UNREGISTER, so that
 *		a REGISTER issued while event-generaion is disabled will
 *		still be processed, and events generated after a RESUME is issued.
 *		Events occuring while PAUSE is in effect are not sent, and not remembered.
 *
 *	   Input parameters: none
 *	   Return Value: none
 *	   Async-signal-safe: no
 *
 *    OMP_REQ_RESUME
 *		Re-enables the generation of any events
 *		Ignored if events are currently not disabled.
 *
 *	   Input parameters: none
 *	   Return Value: none
 *	   Async-signal-safe: no
 *
 */


/******************************************************************
 *  Return Codes in the 'ec' Field
 ******************************************************************/

typedef enum {
	OMP_ERRCODE_OK = 0,		/* successful */ 
	OMP_ERRCODE_ERROR = 1,		/* size is not right or mem is not aligned */
	OMP_ERRCODE_UNKNOWN = 2,	/* request unknown */
	OMP_ERRCODE_UNSUPPORTED = 3,	/* request is known, but not supported */
	OMP_ERRCODE_SEQUENCE_ERR = 4,	/* request was made at the wrong time */
	OMP_ERRCODE_OBSOLETE = 5,	/* request is obsolete, and no longer supported */
	OMP_ERRCODE_THREAD_ERR = 6,	/* request was made from an inappropriate thread */
	OMP_ERRCODE_MEM_TOO_SMALL = 7,	/* return data will not fit in the allocated space */
	OMP_ERRCODE_LAST
} OMP_COLLECTORAPI_EC;



/******************************************************************
 *  Events Subject to Notification
 ******************************************************************/

typedef enum {
	OMP_EVENT_FORK = 1,
	OMP_EVENT_JOIN = 2,
	OMP_EVENT_THR_BEGIN_IDLE = 3,
	OMP_EVENT_THR_END_IDLE = 4,
	OMP_EVENT_THR_BEGIN_IBAR = 5,
	OMP_EVENT_THR_END_IBAR = 6,
	OMP_EVENT_THR_BEGIN_EBAR = 7,
	OMP_EVENT_THR_END_EBAR = 8,
	OMP_EVENT_THR_BEGIN_LKWT = 9,
	OMP_EVENT_THR_END_LKWT = 10,
	OMP_EVENT_THR_BEGIN_CTWT = 11,
	OMP_EVENT_THR_END_CTWT = 12,
	OMP_EVENT_THR_BEGIN_ODWT = 13,
	OMP_EVENT_THR_END_ODWT = 14,
	OMP_EVENT_THR_BEGIN_MASTER = 15,
	OMP_EVENT_THR_END_MASTER = 16,
	OMP_EVENT_THR_BEGIN_SINGLE = 17,
	OMP_EVENT_THR_END_SINGLE = 18,
	OMP_EVENT_THR_BEGIN_ORDERED = 19,
	OMP_EVENT_THR_END_ORDERED = 20,
	OMP_EVENT_THR_BEGIN_ATWT = 21,
	OMP_EVENT_THR_END_ATWT = 22,
	    /*
	     * implementation-specific events may be defined with any enum value
	     * equal to or greater than OMP_EVENT_THR_RESERVED_IMPL
	     */
	OMP_EVENT_THR_RESERVED_IMPL = 4096,
	OMP_EVENT_LAST
} OMP_COLLECTORAPI_EVENT;

/*
 *  All events are generated synchronously to the thread in which they occur
 *	The callback routine is passed a single parameter, the event type
 *
 *  Mandatory event -- must be supported in any conforming implementation
 *
 *    OMP_EVENT_FORK
 *		The master thread is about to fork a team of worker threads
 *
 *  Optional events -- may or may not be supported
 *	The cost of generating these events may be significant, and distort
 *	the performance of the application.
 *
 *    OMP_EVENT_JOIN
 *		All threads in a parallel region have completed their
 *		work, and the master has returned to its serial processing.
 *		For nested parallelism, the "serial" processing may be as
 *		a slave thread to an outer parallel region.
 *
 *    OMP_EVENT_THR_BEGIN_IDLE
 *		A thread enters the THR_IDLE_STATE; a slave thread finishes
 *		work in a parallel region, and rejoins the pool of available
 *		threads.
 *
 *    OMP_EVENT_THR_END_IDLE
 *		A thread leaves the THR_IDLE_STATE
 *
 *    OMP_EVENT_THR_BEGIN_IBAR
 *		A thread enters the THR_IBAR_STATE
 *
 *    OMP_EVENT_THR_END_IBAR
 *		A thread leaves the THR_IBAR_STATE
 *
 *    OMP_EVENT_THR_BEGIN_EBAR
 *		A thread enters the THR_EBAR_STATE
 *
 *    OMP_EVENT_THR_END_EBAR
 *		A thread leaves the THR_EBAR_STATE
 *
 *    OMP_EVENT_THR_BEGIN_LKWT
 *		A thread enters the THR_LKWT_STATE
 *
 *    OMP_EVENT_THR_END_LKWT
 *		A thread leaves the THR_LKWT_STATE
 *
 *    OMP_EVENT_THR_BEGIN_CTWT
 *		A thread enters the THR_CTWT_STATE
 *
 *    OMP_EVENT_THR_END_CTWT
 *		A thread leaves the THR_CTWT_STATE
 *
 *    OMP_EVENT_THR_BEGIN_ODWT
 *		A thread enters the THR_ODWT_STATE
 *
 *    OMP_EVENT_THR_END_ODWT
 *		A thread leaves the THR_ODWT_STATE
 *
 *    OMP_EVENT_THR_BEGIN_MASTER
 *		A thread enters a MASTER region
 *
 *    OMP_EVENT_THR_END_MASTER
 *		A thread leaves a MASTER region
 *
 *    OMP_EVENT_THR_BEGIN_SINGLE
 *		A thread enters a SINGLE region
 *
 *    OMP_EVENT_THR_END_SINGLE
 *		A thread leaves a SINGLE region
 *
 *    OMP_EVENT_THR_BEGIN_ORDERED
 *		A thread enters an ORDERED region
 *
 *    OMP_EVENT_THR_END_ORDERED
 *		A thread leaves an ORDERED region
 *
 *    OMP_EVENT_THR_BEGIN_ATWT
 *		A thread begins waiting to enter an atomic region
 *
 *    OMP_EVENT_THR_END_ATWT
 *		A thread ends waiting to enter an atomic region
 *
 */


/******************************************************************
 *  Thread States
 ******************************************************************/

typedef enum {
    THR_OVHD_STATE = 1,		/* Overhead */
    THR_WORK_STATE = 2,		/* Useful work, excluding reduction, master, single, critical */
    THR_IBAR_STATE = 3,    	/* In an implicit barrier */
    THR_EBAR_STATE = 4,    	/* In an explicit barrier */
    THR_IDLE_STATE = 5,		/* Slave waiting for work */
    THR_SERIAL_STATE = 6,	/* thread not in any OMP parallel region (initial thread only) */
    THR_REDUC_STATE = 7,	/* Reduction */
    THR_LKWT_STATE = 8,		/* Waiting for lock */
    THR_CTWT_STATE = 9,		/* Waiting to enter critical region */
    THR_ODWT_STATE = 10,	/* Waiting to execute an ordered region */
    THR_ATWT_STATE = 11,	/* Waiting to enter an atomic region */

    THR_LAST_STATE
} OMP_COLLECTOR_API_THR_STATE;

/*
 *
 * The "main" thread is the initial thread starting the user code.
 *
 * "user thread" refers to either the initial thread, or a non-OpenMP thread
 *	created explicitly by the user using either the POSIX thread or
 *	Solaris thread thread-creation interface.
 *
 * "OpenMP thread" refers to a thread created by the OpenMP runtime library.
 *
 * THR_OVHD_STATE
 *	A thread (user or OpenMP) is inside an OMP parallel region and is:
 *	  . preparing for the parallel region; or
 *	  . preparing for a new worksharing region; or
 *	  . computing loop iterations; or
 *	  . doing some other overhead work inside a parallel region
 *
 *
 * THR_WORK_STATE 
 *	A thread is inside an OMP parallel region and doing
 *	  "useful" work.  Useful work does *not* include
 *	   working on combining reduction results.
 *
 *
 * THR_SERIAL_STATE
 *	A user thread is outside any OMP parallel region.
 *
 *
 * THR_IDLE_STATE
 *	A non-user thread is waiting to work on an OMP parallel region.
 *
 *
 * THR_IBAR_STATE
 *	A thread is waiting at an implicit barrier at the end 
 *	  of an OMP parallel region, a worksharing region, or a single
 *	  region.
 *
 *
 * THR_EBAR_STATE
 *	A thread is waiting in an explicit barrier. 
 * 
 *	This state is entered when the thread encounters a barrier
 *	region.
 *
 *
 * THR_REDUC_STATE
 *	A thread is working on combining reduction results.
 *
 * 
 * THR_LKWT_STATE
 *	A thread is waiting for an OMP lock.
 * 
 *	This state is entered when the thread encounters a call to 
 *	omp_set_lock() or omp_set_nest_lock(), and the lock it is trying 
 *	to set is already locked.
 * 
 *
 * THR_CTWT_STATE
 *	A thread is waiting to entering an OMP critical region.
 *
 *	This state is entered when the thread encounters a critical
 *	region, and the lock associated with that critical region is
 *	already locked.
 *
 *
 * THR_ODWT_STATE
 *	A thread is waiting to execute an ordered region.
 *
 *	This state is entered when the thread encounters an ordered
 *	region and it cannot execute the ordered region, because some
 *	previous iteration that contains an ordered region has not 
 *	completed yet.
 * 
 * THR_ATWT_STATE
 *	A thread is waiting to enter an OMP atomic region
 *
 *	This state is entered when the thread encounters an
 *	atomic region, and the lock associated with that region (if any)
 *	is held by another thread.
 *
 * There is no state corresponding to being in a critical section,
 *	holding a lock, being in a MASTER section, or a SINGLE
 *	section, or an ORDERED section.  These constructs can be
 *	nested, so a single state is inappropriate.
 */


#ifdef __cplusplus
}
#endif

#endif	/* _OMP_COLLECTOR_API_H */
