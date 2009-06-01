// Switcher
// Copyright © 1999-2001 Stas Maximov
// All rights reserved
// Version 1.0.4

//#include <Application.h>
#include <stdio.h>
#include <List.h>
#include <View.h>
#include <OS.h>
#include <Roster.h>
#include <SupportDefs.h>
#include <MessageFilter.h>
#include <Message.h>
#include <Beep.h>
#include <File.h>
#include <Directory.h>
#include <FindDirectory.h>
#include <NodeMonitor.h>
#include <Entry.h>
#include <InputServerDevice.h>
#include "SwitchFilter.h"
#include "Settings.h"
//#include "KeyTable.h"
#include <InputServerFilter.h>
#include <Locker.h>
#include <syslog.h>

// hot-keys
const uint32	KEY_LCTRL_SHIFT = 0x2000;
const uint32	KEY_OPT_SHIFT = 0x2001;
const uint32	KEY_ALT_SHIFT = 0x2002;
const uint32	KEY_SHIFT_SHIFT = 0x2003;

#if 0 // disable for a time 
const int32 key_table[31][2] = {
{41,101},
{42,114},
{43,116},
{44,121},
{45,117},
{46,105},
{47,111},
{48,112},
{49,91},
{50,93},
{60,97},
{61,115},
{62,100},
{63,102},
{64,103},
{65,104},
{66,106},
{67,107},
{68,108},
{69,59},
{70,39},
{76,122},
{77,120},
{78,99},
{79,118},
{80,98},
{81,110},
{82,109},
{83,44},
{84,46},
{85,47}
};
#endif

enum __msgs {
	MSG_CHANGEKEYMAP = 0x400, // thats for Indicator, don't change it
//	MSG_RELOAD_DESKBAR = 0x1000
}; 

//#undef NDEBUG
//#define DESKBAR_SIGNATURE "application/x-vnd.Be-TSKB"
#define INDICATOR_SIGNATURE "application/x-vnd.KeymapSwitcher"

#if 0 //def NDEBUG
#define trace(x...) syslog(0, __PRETTY_FUNCTION__);\
					syslog(0, x);\
					syslog(0, "\n");
/*
//{ \
//	if (NULL != s) { \
//		BFile file("/boot/home/Switcher.log", B_CREATE_FILE|B_WRITE_ONLY|B_OPEN_AT_END); \
//		file.Write(__PRETTY_FUNCTION__, strlen(__PRETTY_FUNCTION__)); \
//		file.Write(":\t", strlen(":\t")); \
//		file.Write(s, strlen(s)); \
//		file.Write("\n", strlen("\n")); \
//	} \
//
*/
//}
#else
#define trace(s) ((void)0)  
#endif

SettingsMonitor::SettingsMonitor(const char *name, Settings *settings) : BLooper(name) {
	trace("creating monitor");
	this->settings = settings; // copy pointer

	BLocker lock;
	lock.Lock();
	node_ref nref;
	BEntry entry(settings->GetPath().String(), false);
	if(B_OK != entry.InitCheck())
		trace("entry is invalid!");
	entry.GetNodeRef(&nref);
	watch_node(&nref, B_WATCH_STAT | B_WATCH_NAME, this);
	lock.Unlock();
	trace("created");
}

SettingsMonitor::~SettingsMonitor() {
	trace("killing");
	BLocker lock;
	lock.Lock();
	node_ref nref;
	BEntry entry(settings->GetPath().String(), false);
	if(B_OK != entry.InitCheck())
		trace("entry is invalid!");
	entry.GetNodeRef(&nref);
	watch_node(&nref, B_STOP_WATCHING, this);
	lock.Unlock();
	trace("killed");
}

void SettingsMonitor::MessageReceived(BMessage *message) {
	trace("start");
	switch (message->what) {
		case B_NODE_MONITOR: {
			BLocker lock;
			lock.Lock();
			settings->Reload();
			lock.Unlock();
			return; // message handled
		}
	/*	case MSG_RELOAD_DESKBAR: {
			kill_team(be_roster->TeamFor(DESKBAR_SIGNATURE));
			while (be_roster->IsRunning(DESKBAR_SIGNATURE)) 
				snooze(100);
			be_roster->Launch(DESKBAR_SIGNATURE);
			while (!be_roster->IsRunning(DESKBAR_SIGNATURE)) 
				snooze(100);
			be_roster->Launch(INDICATOR_SIGNATURE);
		}*/
	}
	BLooper::MessageReceived(message);
	trace("end");
}


BInputServerFilter* instantiate_input_filter() {
	openlog("switcher_filter", LOG_CONS, LOG_USER);
	trace("start");
	// this is where it all starts
	// make sure this function is exported!
	trace("filter instantiated");
	SwitchFilter *filter = new SwitchFilter();
	if (filter) {
		return filter;
	}
	return (NULL);
}


SwitchFilter::SwitchFilter() : BInputServerFilter() {
	trace("start");

	// register your filter(s) with the input_server
	// the last filter in the list should be NULL
//	trace(Name());
	settings = NULL;
	monitor = NULL;
	switch_on_hold = false;
}


// Destructor
SwitchFilter::~SwitchFilter() {
	trace("start");

	// do some clean-ups here
	if (NULL != monitor)
		monitor->PostMessage(B_QUIT_REQUESTED);
	snooze(1000000); // lets wait until monitor dies
	
	if (NULL != settings) {
		trace("killing settings now");
		settings->Reload(); // we need to load new settings first
		delete settings; // cause destructor saves settings here <g>
	}
	trace("end");
}


// Init check
status_t SwitchFilter::InitCheck() {
	trace("init check");
	settings = new Settings("Switcher");
	monitor = new SettingsMonitor(INDICATOR_SIGNATURE, settings);
	monitor->Run();

	// check if flag to restart Deskbar is there
/*	BEntry entry("/boot/home/Switcher.tmp", false);
	if(entry.Exists()) {
		// its really nice to do this in different thread to let input_server to get up faster
		monitor->PostMessage(new BMessage(MSG_RELOAD_DESKBAR));
		// don't need flag anymore
		entry.Remove(); 
	}*/
	
	return B_OK;
}



// Filter key pressed 
filter_result SwitchFilter::Filter(BMessage *message, BList *outList) {
	switch (message->what) {
	case B_KEY_MAP_CHANGED:
		trace("key_map_changed");
		break;

	case B_MODIFIERS_CHANGED: {
		uint32 new_modifiers = 0;
		uint32 old_modifiers = 0;
		uint8 states = 0;

////		key_info info;
////		get_key_info(&info);
////		new_modifiers = info.modifiers;
		message->FindInt32("modifiers", (int32 *) &new_modifiers);
		message->FindInt32("be:old_modifiers", (int32 *) &old_modifiers);
		message->FindInt8("states", (int8 *)&states);

		old_modifiers &= ~(B_CAPS_LOCK | B_SCROLL_LOCK | B_NUM_LOCK);
		new_modifiers &= ~(B_CAPS_LOCK | B_SCROLL_LOCK | B_NUM_LOCK);

		char *buf = new char[128];
		sprintf(buf, "new: 0x%04X, old: 0x%04X", new_modifiers, old_modifiers);
		trace(buf);
		delete buf;
		if(switch_on_hold)
			trace("switch is on hold now");					
		if(switch_on_hold && new_modifiers == 0) {
			UpdateIndicator();
			switch_on_hold = false;
			return B_SKIP_MESSAGE;
		}
		int32 hotkey;
		settings->FindInt32("hotkey", &hotkey);
		
		switch (hotkey) {
		case KEY_LCTRL_SHIFT: {
			old_modifiers &= 0x0FF; // cut off all RIGHT and LEFT key bits
			uint32 mask1 = B_SHIFT_KEY|B_CONTROL_KEY;
			uint32 mask2 = B_SHIFT_KEY|B_OPTION_KEY;
			if ((old_modifiers == mask1) || (old_modifiers == mask2))
				switch_on_hold = true;
			return B_SKIP_MESSAGE;
		}
		
		case KEY_OPT_SHIFT:
		case KEY_ALT_SHIFT: {
			old_modifiers &= 0x0FF; // cut off all RIGHT and LEFT key bits
			uint32 mask1 = B_SHIFT_KEY|B_COMMAND_KEY;
			if (old_modifiers == mask1)
				switch_on_hold = true;
			return B_SKIP_MESSAGE;
		} 
		case KEY_SHIFT_SHIFT: {
			uint32 mask1 = B_SHIFT_KEY|B_LEFT_SHIFT_KEY|B_RIGHT_SHIFT_KEY;
			if (old_modifiers == mask1)
				switch_on_hold = true;
			return B_SKIP_MESSAGE;
		}
		default:
			break;
		}
		break; // B_MODIFIERS_CHANGED
	}

	case B_KEY_DOWN: {
		if(switch_on_hold) {
			trace("skipping last modifier change");
			switch_on_hold = false; // skip previous attempt
		}
		
// disable for a time key mapping...		
#if 0 					
		// check if it is Alt+Key key pressed, we shall put correct value 
		// just as it is with American keymap 
		int32 modifiers = message->FindInt32("modifiers");
		int32 raw = message->FindInt32("raw_char");

/*		if ((modifiers & B_CONTROL_KEY) && (raw == B_ESCAPE)) {
			trace("Ctrl+Esc found. Generating message...");
			BMessage msg(B_MOUSE_DOWN);
			msg.AddInt32("clicks", 1);
			msg.AddPoint("where", BPoint(790, 5));
			msg.AddInt32("modifiers", modifiers);
			msg.AddInt32("buttons", B_PRIMARY_MOUSE_BUTTON);
			be_roster->Broadcast(&msg);
			return B_SKIP_MESSAGE;		
		} 
*/
		modifiers &= ~(B_CAPS_LOCK | B_SCROLL_LOCK | B_NUM_LOCK);
		if((modifiers != (B_COMMAND_KEY | B_LEFT_COMMAND_KEY)) && 
			(modifiers != (B_COMMAND_KEY | B_RIGHT_COMMAND_KEY))) {
//			trace("quitting because...");
			break; // only Alts needed
		}
			
		int32 key = message->FindInt32("key");
		
/*		if( key == 94 ) { // request for RealClipboard 
			BMessenger *indicator = GetIndicatorMessenger();
			trace("RealClipboard requested.");
			if(indicator)
				indicator->SendMessage(MSG_SHOW_RC_MENU);
			break;
		}
*/		

		// continue processing "modifiered" key
		app_info appinfo;
		if(B_OK != be_roster->GetActiveAppInfo(&appinfo)) 
			break; // no active app found. strange.. :))

			
		if(raw > 0)
			break;

		for(int32 i=0; i<31;i++) {
			if(key == key_table[i][0])
				raw = key_table[i][1]; // set correct  value
		}
		if(raw < 0) // have no raw value for this key
			break;
			
		message->ReplaceInt32("raw_char", raw);
#endif
		break;
	} 
	default: 
		break;
	} // switch
	return B_DISPATCH_MESSAGE; // tell input_server to dispatch message
}


// Gets Indicator's BMessenger
BMessenger* SwitchFilter::GetIndicatorMessenger() {
	const char *REPLICANT_NAME = "Switcher/Deskbar";
	BMessenger *indicator = 0;

	BMessage	request(B_GET_PROPERTY);
	BMessenger	to;
	BMessenger	status;

	request.AddSpecifier("Messenger");
	request.AddSpecifier("Shelf");
	
	// In the Deskbar the Shelf is in the View "Status" in Window "Deskbar"
//	request.AddSpecifier("View", REPLICANT_NAME);
	request.AddSpecifier("View", "Status");
	request.AddSpecifier("Window", "Deskbar");
	to = BMessenger("application/x-vnd.Be-TSKB", -1);
	
	BMessage	reply;
	
	if (to.SendMessage(&request, &reply) == B_OK) {
		if(reply.FindMessenger("result", &status) == B_OK) {

			// enum replicant in Status view
			int32	index = 0;
			int32	uid;
			while ((uid = GetReplicantAt(status, index++)) >= B_OK) {
				BMessage	rep_info;
				if (GetReplicantName(status, uid, &rep_info) != B_OK) {
					continue;
				}
				const char *name;
				if (rep_info.FindString("result", &name) == B_OK) {
					if(strcmp(name, REPLICANT_NAME)==0) {
						BMessage rep_view;
						if (GetReplicantView(status, uid, &rep_view)==0) {
							BMessenger result;
							if (rep_view.FindMessenger("result", &result) == B_OK) {
								indicator = new BMessenger(result);
							}
						} 
					}
				}
			}
		}
	}
	return indicator;
}

//
void SwitchFilter::UpdateIndicator() {
	trace("go");
	// tell Indicator to change icon
	BMessenger *indicator = GetIndicatorMessenger();
	if(indicator)
		indicator->SendMessage(MSG_CHANGEKEYMAP);
}

//
int32 SwitchFilter::GetReplicantAt(BMessenger target, int32 index) const
{
	/*
	 So here we want to get the Unique ID of the replicant at the given index
	 in the target Shelf.
	 */
	 
	BMessage	request(B_GET_PROPERTY);		// We're getting the ID property
	BMessage	reply;
	status_t	err;
	
	request.AddSpecifier("ID");					// want the ID
	request.AddSpecifier("Replicant", index);	// of the index'th replicant
	
	if ((err = target.SendMessage(&request, &reply)) != B_OK)
		return err;
	
	int32	uid;
	if ((err = reply.FindInt32("result", &uid)) != B_OK) 
		return err;
	
	return uid;
}


//
status_t SwitchFilter::GetReplicantName(BMessenger target, int32 uid, BMessage *reply) const
{
	/*
	 We send a message to the target shelf, asking it for the Name of the 
	 replicant with the given unique id.
	 */
	 
	BMessage	request(B_GET_PROPERTY);
	BMessage	uid_specifier(B_ID_SPECIFIER);	// specifying via ID
	status_t	err;
	status_t	e;
	
	request.AddSpecifier("Name");		// ask for the Name of the replicant
	
	// IDs are specified using code like the following 3 lines:
	uid_specifier.AddInt32("id", uid);
	uid_specifier.AddString("property", "Replicant");
	request.AddSpecifier(&uid_specifier);
	
	if ((err = target.SendMessage(&request, reply)) != B_OK)
		return err;
	
	if (((err = reply->FindInt32("error", &e)) != B_OK) || (e != B_OK))
		return err ? err : e;
	
	return B_OK;
}

//
status_t SwitchFilter::GetReplicantView(BMessenger target, int32 uid, BMessage *reply) const
{
	/*
	 We send a message to the target shelf, asking it for the Name of the 
	 replicant with the given unique id.
	 */
	 
	BMessage	request(B_GET_PROPERTY);
	BMessage	uid_specifier(B_ID_SPECIFIER);	// specifying via ID
	status_t	err;
	status_t	e;
	
	request.AddSpecifier("View");		// ask for the Name of the replicant
	
	// IDs are specified using code like the following 3 lines:
	uid_specifier.AddInt32("id", uid);
	uid_specifier.AddString("property", "Replicant");
	request.AddSpecifier(&uid_specifier);
	
	if ((err = target.SendMessage(&request, reply)) != B_OK)
		return err;
	
	if (((err = reply->FindInt32("error", &e)) != B_OK) || (e != B_OK))
		return err ? err : e;
	
	return B_OK;
}

