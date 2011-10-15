//---------------------------------------------------------------------
//  Copyright (c) 2009 Mike Stilman
//  All Rights Reserved.
//
//  Permission to duplicate or use this software in whole or in part
//  is only granted by consultation with the author.
//
//    Mike Stilman              mstilman@cc.gatech.edu
//
//	  Robotics and Intelligent Machines
//    Georgia Tech
//--------------------------------------------------------------------
#include <wx/wx.h>
#include <iostream>

#include "../GUI/Viewer.h"
#include "../GUI/GUI.h"
#include "../GUI/RSTSlider.h"
#include "../GUI/RSTFrame.h"


#include "PlanningTab.h"

using namespace std;

/* Quick intro to adding tabs:
 * 1- Copy template cpp and header files and replace with new class name
 * 2- include classname.h in AllTabs.h, and use the ADD_TAB macro to create it
 */

// Control IDs (used for event handling - be sure to start with a non-conflicted id)
enum planTabEvents {
	button_SetStart = 50,
	button_SetGoal,
	button_showStart,
	button_showGoal,
	button_resetPlanner,
	button_empty1,
	button_empty2,
	button_Plan,
	button_Stop,
	button_UpdateTime,
	button_ExportSequence,
	button_ShowPath,
	checkbox_beGreedy,
	checkbox_useConnect,
	checkbox_showProgress,
	slider_Time
};

// sizer for whole tab
wxBoxSizer* sizerFull;

//Add a handler for any events that can be generated by the widgets you add here (sliders, radio, checkbox, etc)
BEGIN_EVENT_TABLE(PlanningTab, wxPanel)
EVT_COMMAND (wxID_ANY, wxEVT_RST_SLIDER_CHANGE, PlanningTab::OnSlider)
EVT_COMMAND (wxID_ANY, wxEVT_COMMAND_RADIOBOX_SELECTED, PlanningTab::OnRadio)
EVT_COMMAND (wxID_ANY, wxEVT_COMMAND_BUTTON_CLICKED, PlanningTab::OnButton)
EVT_COMMAND (wxID_ANY, wxEVT_COMMAND_CHECKBOX_CLICKED, PlanningTab::OnCheckBox)
END_EVENT_TABLE()

// Class constructor for the tab: Each tab will be a subclass of RSTTab
IMPLEMENT_DYNAMIC_CLASS(PlanningTab, RSTTab)

PlanningTab::PlanningTab(wxWindow *parent, const wxWindowID id,
		const wxPoint& pos, const wxSize& size, long style) :
	RSTTab(parent, id, pos, size, style) {

	robotID = 0;

	sizerFull = new wxBoxSizer(wxHORIZONTAL);

	// ** Create left static box for configuring the planner **

	// Create StaticBox container for all items
	wxStaticBox* configureBox = new wxStaticBox(this, -1, wxT("Configure"));

	// Create sizer for this box with horizontal layout
	wxStaticBoxSizer* configureBoxSizer = new wxStaticBoxSizer(configureBox, wxHORIZONTAL);

	// Create a sizer for radio buttons in 1st column
	wxBoxSizer *col1Sizer = new wxBoxSizer(wxVERTICAL);
	wxBoxSizer *miniSizer = new wxBoxSizer(wxVERTICAL); // annoying hack to get checkboxes close together
	miniSizer->Add(new wxCheckBox(this, checkbox_beGreedy, _T("&Goal Bias")),
			1, // vertical stretch evenly
			wxALIGN_NOT,
			0);
	miniSizer->Add(new wxCheckBox(this, checkbox_useConnect, _T("Use &Connect algorithm")),
			1, // vertical stretch evenly
			wxALIGN_NOT,
			0);
	miniSizer->Add(new wxCheckBox(this, checkbox_showProgress, _T("Show &Progress")),
				1, // vertical stretch evenly
				wxALIGN_NOT,
				0);
	col1Sizer->Add(miniSizer,1,wxALIGN_NOT,0);
	// Create radio button for rrt_style
	static const wxString RRTStyles[] =
	{
		wxT("Single"),
		wxT("Bi-directional")
	};
	col1Sizer->Add(new wxRadioBox(this, wxID_ANY, wxT("RRT &style:"),
			wxDefaultPosition, wxDefaultSize, WXSIZEOF(RRTStyles), RRTStyles, 1,
			wxRA_SPECIFY_ROWS),
			1, // stretch evenly with buttons and checkboxes
			wxALIGN_NOT,
			0);
	// Add col1 to configureBoxSizer
	configureBoxSizer->Add(col1Sizer,
			3, // 3/5 of configure box
			wxALIGN_NOT,
			0); //

	// Create sizer for start buttons in 2nd column
	wxBoxSizer *col2Sizer = new wxBoxSizer(wxVERTICAL);
	col2Sizer->Add(new wxButton(this, button_SetStart, wxT("Set &Start")),
			0, // make horizontally unstretchable
			wxALL, // make border all around (implicit top alignment)
			1); // set border width to 1, so start buttons are close together
	col2Sizer->Add(new wxButton(this, button_showStart, wxT("Show S&tart")),
			0, // make horizontally unstretchable
			wxALL, // make border all around (implicit top alignment)
			1); // set border width to 1, so start buttons are close together
	col2Sizer->Add(new wxButton(this, button_empty1, wxT("Empty 1")),
			0, // make horizontally unstretchable
			wxALL, // make border all around (implicit top alignment)
			1); // set border width to 1, so start buttons are close together


	// Add col2Sizer to the configuration box
	configureBoxSizer->Add(col2Sizer,
			1, // takes half the space of the configure box
			wxALIGN_NOT); // no border and center horizontally

	// Create sizer for goal buttons in 3rd column
	wxBoxSizer *col3Sizer = new wxBoxSizer(wxVERTICAL);
	col3Sizer->Add(new wxButton(this, button_SetGoal, wxT("Set &Goal")),
			0, // make horizontally unstretchable
			wxALL, // make border all around (implicit top alignment)
			1); // set border width to 1, so start buttons are close together
	col3Sizer->Add(new wxButton(this, button_showGoal, wxT("Show G&oal")),
			0, // make horizontally unstretchable
			wxALL, // make border all around (implicit top alignment)
			1); // set border width to 1, so start buttons are close together
	col3Sizer->Add(new wxButton(this, button_empty2, wxT("Empty 2")),
			0, // make horizontally unstretchable
			wxALL, // make border all around (implicit top alignment)
			1); // set border width to 1, so start buttons are close together
	configureBoxSizer->Add(col3Sizer,
			1, // size evenly with radio box and checkboxes
			wxALIGN_NOT); // no border and center horizontally

	// Add this box to parent sizer
	sizerFull->Add(configureBoxSizer,
			4, // 4-to-1 ratio with execute sizer, since it just has 3 buttons
			wxEXPAND | wxALL,
			6);


	// ** Create right static box for running the planner **
	wxStaticBox* executeBox = new wxStaticBox(this, -1, wxT("Execute Planner"));

	// Create sizer for this box
	wxStaticBoxSizer* executeBoxSizer = new wxStaticBoxSizer(executeBox, wxVERTICAL);

	// Add buttons for "plan", "save movie", and "show path"
	executeBoxSizer->Add(new wxButton(this, button_Plan, wxT("&Start")),
			1, // stretch to fit horizontally
			wxGROW); // let it hog all the space in it's column

	executeBoxSizer->Add(new wxButton(this, button_Stop, wxT("&Stop")),
			1, // stretch to fit horizontally
			wxGROW);


	wxBoxSizer *timeSizer = new wxBoxSizer(wxHORIZONTAL);
	timeText = new wxTextCtrl(this,1008,wxT("5.0"),wxDefaultPosition,wxSize(40,20),wxTE_RIGHT);//,wxTE_PROCESS_ENTER | wxTE_RIGHT);
	timeSizer->Add(timeText,2,wxALL,1);
	timeSizer->Add(new wxButton(this, button_UpdateTime, wxT("Set T(s)")),2,wxALL,1);
	executeBoxSizer->Add(timeSizer,1,wxALL,2);

	executeBoxSizer->Add(new wxButton(this, button_ShowPath, wxT("&Print")),
			1, // stretch to fit horizontally
			wxGROW);

	sizerFull->Add(executeBoxSizer, 1, wxEXPAND | wxALL, 6);

	SetSizer(sizerFull);

	rrtStyle = 0;
	greedyMode = false;
	connectMode = false;
	showProg = false;

}

// Handle Radio toggle
void PlanningTab::OnRadio(wxCommandEvent &evt) {
	// Add code here to implement a bi-directional RRT

	rrtStyle = evt.GetSelection();
	cout << "rrtStyle = " << rrtStyle << endl;
}

// Handle Button Events
void PlanningTab::OnButton(wxCommandEvent &evt) {


	int button_num = evt.GetId();
	switch (button_num) {
	case button_SetStart:

		break;

	case button_SetGoal:

		break;

	case button_showStart:

		break;

	case button_showGoal:

		break;

/*
	case button_resetPlanner:
		if (world != NULL) {
			if (planner != NULL)
				delete planner;

			cout << "Creating a new planner" << endl;
			planner = new Planner(world, 0, greedyMode, connectMode, showProg, rrtStyle);
		} else {
			cout << "must have a world loaded to make a planner" << endl;
		}
		break;
*/
	case button_empty1:
		cout << "Empty Button to use for whatever you want" << endl;

		break;

	case button_empty2:
		cout << "Empty Button to use for whatever you want" << endl;

		break;

	case button_Plan:

		break;

	case button_ShowPath:

		break;
	}
}

void PlanningTab::SetTimeline(int robot, vector<int> links, list<Eigen::VectorXd> path){

}

// Handle CheckBox Events
void PlanningTab::OnCheckBox(wxCommandEvent &evt) {
	int checkbox_num = evt.GetId();

	switch (checkbox_num) {
	case checkbox_beGreedy:
		greedyMode = (bool)evt.GetSelection();
		cout << "greedy = " << greedyMode << endl;
		break;

	case checkbox_useConnect:
		connectMode = (bool)evt.GetSelection();
		cout << "useConnect = " << connectMode << endl;
		break;
	case checkbox_showProgress:
			showProg = (bool)evt.GetSelection();
			cout << "showProg = " << showProg << endl;
			break;
	}
}

//Handle slider changes
void PlanningTab::OnSlider(wxCommandEvent &evt) {
	if (selectedTreeNode == NULL) {
		return;
	}

	int slnum = evt.GetId();
	double pos = *(double*) evt.GetClientData();
	char numBuf[1000];

	switch (slnum) {
	case slider_Time:
		sprintf(numBuf, "X Change: %7.4f", pos);
		cout << "Timeline slider output: " << numBuf << endl;
		//handleTimeSlider(); // uses slider position to query plan state
		break;

	default:
		return;
	}
	//world->updateCollision(o);
	//viewer->UpdateCamera();

	if (frame != NULL)
		frame->SetStatusText(wxString(numBuf, wxConvUTF8));
}

// This function is called when an object is selected in the Tree View or other
// global changes to the RST world. Use this to capture events from outside the tab.
void PlanningTab::RSTStateChange() {
	if (selectedTreeNode == NULL) {
		return;
	}

	string statusBuf;
	string buf, buf2;
	switch (selectedTreeNode->dType) {
	case Return_Type_Object:

		// Enter action for object select events here:

		break;
	case Return_Type_Robot:


		// Enter action for Robot select events here:

		break;
	case Return_Type_Link:

		break;
    default:
        fprintf(stderr, "Someone else's problem!\n");
        assert(0);
        exit(1);
	}
	//cout << buf << endl;
	frame->SetStatusText(wxString(statusBuf.c_str(), wxConvUTF8));
	sizerFull->Layout();
}

