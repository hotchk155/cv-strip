////////////////////////////////////////////////////////////
//
// MINI MIDI CV
//
// GATE OUTPUTS
//
////////////////////////////////////////////////////////////

// INCLUDE FILES
#include <system.h>
#include "cv-strip.h"

//
// CONSTANTS
//

// Define bits for the shift register outputs
#define SRB_GATA	0x0004
#define SRB_GATB 	0x0002
#define SRB_CLKA	0x0008
#define SRB_CLKB	0x0001
#define SRB_DRM1 	0x0100
#define SRB_DRM2 	0x0200
#define SRB_DRM3 	0x0400
#define SRB_DRM4 	0x0800
#define SRB_DRM5 	0x1000
#define SRB_DRM6 	0x2000
#define SRB_DRM7 	0x4000
#define SRB_DRM8 	0x8000

// List of modes for a gate output to be triggered
enum {
	GATE_OFF,
	
	// respond to events from a note stack
	GATE_NOTE_ON,			// when any note on is received
	GATE_NOTE_ACCENT,		// when a note is received above accent threshold
	GATE_NOTE_OFF,			// when no notes are pressed		
	GATE_NOTE_GATEA,		// when note out A is playing
	GATE_NOTE_GATEB,		// when note out B is playing
	GATE_NOTE_GATEC,		// when note out C is playing
	GATE_NOTE_GATED,		// when note out D is playing
		
	// Respond to raw MIDI events
	GATE_MIDI_NOTE,
	GATE_MIDI_CC,
	GATE_MIDI_CLOCK,
	GATE_MIDI_TRANSPORT,
};

// Types of clock trigger
enum 
{
	GATE_CLOCK_TICK,		// triggered by divided tick count
	GATE_CLOCK_RUNNING		// triggered by MIDI clock running state
};

//
// STRUCT DEFS
//

// Structure to hold mapping of a note stack event to a gate
typedef struct {
	byte stack_id;	// index of the note stack
} T_GATE_EVENT;

// Structure to hold mapping of a raw MIDI note to a gate
typedef struct {
	byte chan;			// midi channel
	byte note;			// note range: lowest note
	byte note_max;		// note range: highest note (0 if there is only one note)
	byte vel_min;		// minimum velocity that will trigger the gate
} T_GATE_MIDI_NOTE;

// Structure to hold mapping of a raw MIDI CC to a gate
typedef struct {
	byte chan;			// midi channel
	byte cc;			// CC number
	byte threshold;		// threshold for gate ON
	byte last_value;	// (STATE) last CC value
} T_GATE_MIDI_CC;

// Structure to hold mapping of MIDI clock to a gate
typedef struct {
	byte type;			// type of event - GATE_CLOCK_xxx
	byte div;			// clock divider (@24ppqn)
	byte ticks;			// (STATE) number of ticks counted
} T_GATE_MIDI_CLOCK;

// The gate out structure which combines the above
typedef struct {
	byte mode;			// type of trigger - GATE_xxx enum
	byte duration;		// gate pulse duration in ms (or 0 for "as long as active")
	union {
		T_GATE_EVENT 		event;
		T_GATE_MIDI_NOTE 	note;
		T_GATE_MIDI_CC 		cc;
		T_GATE_MIDI_CLOCK	clock;
	}
	byte counter;		// (STATE) pulse duration counter
} GATE_OUT;

//
// FILE SCOPE DATA
//

// Shift register status
static unsigned int g_sr_data = 0;

// Gate status
static GATE_OUT g_gate[NUM_GATE_OUTS];

////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////
// PRIVATE FUNCTIONS
////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// LOAD GATE SHIFT REGISTER
static void load_gates(unsigned int d) {
	unsigned int m1 = 0x0080;
	unsigned int m2 = 0x8000;
	P_SRLAT = 0;
	while(m1) {
		P_SRCLK = 0;
		P_SRDAT1 = !!(d&m1);
		P_SRDAT2 = !!(d&m2);
		P_SRCLK = 1;
		m1>>=1;
		m2>>=1;
	}
	P_SRLAT = 1;
}

////////////////////////////////////////////////////////////
// TRIGGER OR UNTRIGGER A GATE
static void trigger(GATE_CONFIG *pgate, byte index, byte type)
{
	// get appropriate shift register bit
	unsigned int gate_bit = 0;
	switch(which) {
		case GATE_A: 	gate_bit = SRB_GATA; break;
		case GATE_B: 	gate_bit = SRB_GATB; break;
		case GATE_DRM1: gate_bit = SRB_DRM1; break;
		case GATE_DRM2: gate_bit = SRB_DRM2; break;
		case GATE_DRM3: gate_bit = SRB_DRM3; break;
		case GATE_DRM4: gate_bit = SRB_DRM4; break;
		case GATE_DRM5: gate_bit = SRB_DRM5; break;
		case GATE_DRM6: gate_bit = SRB_DRM6; break;
		case GATE_DRM7: gate_bit = SRB_DRM7; break;
		case GATE_DRM8: gate_bit = SRB_DRM8; break;
		case GATE_CLKA: gate_bit = SRB_CLKA; break;
		case GATE_CLKB: gate_bit = SRB_CLKB; break;
		default: return;
	}	
	
	// apply bit change to shift register data
	unsigned int new_data = g_sr_data;
	if(type) {
		new_data = g_sr_data|gate_bit;
		pgate->counter = pgate->duration;
	}
	else {
		new_data = g_sr_data & ~gate_bit;
		pgate->counter = 0;
	}
	
	// reload shift registers if data changed
	if(new_data != g_sr_data) {
		load_gates(g_sr_data);
		g_sr_data = new_data;
	}
}

////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////
// PUBLIC FUNCTIONS
////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// HANDLE EVENT FROM A NOTE STACK
void gate_event(byte event, byte stack_id)
{
	byte i;
	
	// iterate thru gate outputs
	for(i=0; i<NUM_GATE_OUTS; ++i) {	
		GATE_OUT *pgate = &g_gate[i];
		
		// check this output is watching this note stack
		if(pgate->event.stack_id != stack_id)
			continue;
			
		// check the mode of this gate against the event
		switch(pgate->mode) {
			case GATE_NOTE_ON: // Any note on/changed
				if(EV_NOTE_ON == event) {
					trigger(pgate, i, true);
				}
				else if(EV_NOTE_OFF == event) {
					trigger(pgate, i, false);
				}
				break;
			case GATE_NOTE_OFF: // All notes off
				if(EV_NOTE_OFF == event) {
					trigger(pgate, i, true);
				}
				else if(EV_NOTE_ON == event) {
					trigger(pgate, i, false);
				}
				break;
			case GATE_NOTE_ACCENT: // Any note on with velocity above threshold
				if(EV_NOTE_ON == event) {
					if(g_stack[stack_id].vel >= g_accent_vel) {
						trigger(pgate, i, true);
					}
				}
				else if(EV_NOTE_OFF == event) {					
					trigger(pgate, i, false);
				}
				break;
			case GATE_NOTE_GATEA: // Note present at output A
				if(event == EV_NOTE_A) {
					trigger(pgate, i, true);
				}
				else if(event == EV_NO_NOTE_A) {
					trigger(pgate, i, false);
				}
				break;
			case GATE_NOTE_GATEB: // Note present at output B
				if(event == EV_NOTE_B) {
					trigger(pgate, i, true);
				}
				else if(event == EV_NO_NOTE_B) {
					trigger(pgate, i, false);
				}
				break;
			case GATE_NOTE_GATEC: // Note present at output C
				if(event == EV_NOTE_C) {
					trigger(pgate, i, true);
				}
				else if(event == EV_NO_NOTE_C) {
					trigger(pgate, i, false);
				}
				break;
			case GATE_NOTE_GATED: // Note present at output D
				if(event == EV_NOTE_D) {
					trigger(pgate, i, true);
				}
				else if(event == EV_NO_NOTE_D) {
					trigger(pgate, i, false);
				}
				break;
		}
	}
}

////////////////////////////////////////////////////////////
// HANDLE EVENT FROM RAW MIDI NOTE
// Note on has velocity > 0
void gate_midi_note(byte chan, byte note, byte vel) 
{
	// for each gate output
	for(i=0; i<NUM_GATE_OUTS; ++i) {
		GATE_OUT *pgate = &g_gate[i];
		
		// does this gate respond to midi note?
		if(pgate->mode != GATE_MIDI_NOTE)
			continue;
			
		// does the MIDI channel match?
		switch(pgate->note.chan) {
			case CHAN_OMNI:	
				break;
			case CHAN_GLOBAL:
				if(g_chan != CHAN_OMNI && chan != g_chan) {
					continue;
				}
				break;
			default:
				if(chan != pgate->note.chan) {
					continue;
				}
				break;
		}

		// is the note in range
		if(!note_max && note != note_min) {
			continue;
		}
		if(note < note_min) {
			continue;
		}		
		if(note_max && note > note_max) {
			continue;
		}
		
		// is this a note off or note on with velocity above threshold?
		if(vel && vel < vel_min) {
			continue;
		}		
		
		// trigger (for note on) or untrigger (for note off)
		gate_trigger(pgate, i, !!vel);
	}			
}

////////////////////////////////////////////////////////////
// HANDLE EVENT FROM RAW MIDI CC
void gate_midi_cc(byte chan, byte cc, byte value) 
{
	// for each gate output
	for(i=0; i<NUM_GATE_OUTS; ++i) {
		GATE_OUT *pgate = &g_gate[i];
		
		// does this gate respond to CC?
		if(pgate->mode != GATE_MIDI_CC)
			continue;
		
		// is this the correct CC?	
		if(cc != pgate->cc.cc) {
			continue;
		}		

		// does the channel match the MIDI message?
		switch(pgate->note.chan) {
			case CHAN_OMNI:	
				break;
			case CHAN_GLOBAL:
				if(g_chan != CHAN_OMNI && chan != g_chan) {
					continue;
				}
				break;
			default:
				if(chan != pgate->cc.chan) {
					continue;
				}
				break;
		}
				
		// has the value just gone above threshold?
		if(value >= pgate->cc.threshold &&
			( pgate->last_value < pgate->cc.threshold || last_value == NO_VALUE)) {
			
			// trigger gate
			gate_trigger(pgate, true);
			pgate->last_value = value;		
		}
		// has the value just gone below threshold?
		else if(value < pgate->cc.threshold &&
			( pgate->last_value >= pgate->cc.threshold || last_value == NO_VALUE)) {
			
			// untrigger gate
			gate_trigger(pgate, false);
			pgate->last_value = value;		
		}
	}			
}

////////////////////////////////////////////////////////////
// HANDLE EVENT FROM RAW MIDI CLOCK MESSAGE
void gate_midi_clock(byte msg)
{
	// for each gate
	for(i=0; i<NUM_GATE_OUTS; ++i) {
		GATE_OUT *pgate = &g_gate[i];
		
		// does this gate respond to clock?
		if(pgate->mode != GATE_MIDI_CLOCK)
			continue;

		// handle clock message
		switch(pgate->clock.type) {
		case GATE_CLOCK_TICK: // Gate responds to divided clock tick
			switch(msg) {
			case MIDI_SYNCH_TICK: // CLOCK TICK
				if(++pgate->clock.ticks >= pgate->clock.div) {
					gate_trigger(pgate, true);
					pgate->clock.ticks = 0;
				}
				break;				
			case MIDI_SYNCH_START: // START RESETS COUNTER
				pgate->clock.ticks = 0;
				break;
			}
			break;
		case GATE_CLOCK_RUNNING: // Gate responds to MIDI transport
			switch(msg) {
			case MIDI_SYNCH_START:	
				gate_trigger(pgate, true);
				break;
			case MIDI_SYNCH_CONTINUE:
				gate_trigger(pgate, true);
				break;
			case MIDI_SYNCH_STOP:
				gate_trigger(pgate, false);
				break;
			}
			break;
		}
	}
}

////////////////////////////////////////////////////////////
// MANAGE GATE TIMEOUTS
void gate_run() {
	for(i=0; i<NUM_GATE_OUTS; ++i) {
		GATE_CONFIG *pgate = &g_gate[i];
		if(pgate->counter) {
			if(!--pgate->counter) {
				trigger(pgate, i, false);
			}
		}
	}
}

////////////////////////////////////////////////////////////
// INIT GATE STUFF
void gate_init() {

	// turn all gates off
	g_sr_data = 0;
	load_gates(g_sr_data);
	
	// initialise state info
	for(i=0; i<NUM_GATE_OUTS; ++i) {
		GATE_OUT *pgate = &g_gate[i];
		pgate->counter = 0;
		switch(pgate->mode) {
			GATE_MIDI_CC:
				pgate->cc.last_value = NO_VALUE;
				break;
			GATE_MIDI_CLOCK:
				pgate->clock.ticks = 0;
				break;
			GATE_OFF:
			GATE_MIDI_NOTE:
			GATE_MIDI_TRANSPORT:
			default:
				break;
		}
	}
}
