/*
 * Copyright (c) 2011, Georgia Tech Research Corporation
 * 
 * Humanoid Robotics Lab      Georgia Institute of Technology
 * Director: Mike Stilman     http://www.golems.org
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *     * Redistributions of source code must retain the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials
 *       provided with the distribution.
 *     * Neither the name of the Georgia Tech Research Corporation nor
 *       the names of its contributors may be used to endorse or
 *       promote products derived from this software without specific
 *       prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY GEORGIA TECH RESEARCH CORPORATION ''AS
 * IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL GEORGIA
 * TECH RESEARCH CORPORATION BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "tabRipPlanner.h"

#include <wx/wx.h>
#include <GUI/Viewer.h>
#include <GUI/GUI.h>
#include <GUI/GRIPSlider.h>
#include <GUI/GRIPFrame.h>
#include <Tabs/GRIPTab.h>

#include <iostream>

#include <Tabs/AllTabs.h>
#include <GRIPApp.h>


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
	checkbox_useSmooth,
	slider_Time
};

// sizer for whole tab
wxBoxSizer* sizerFull;

//Add a handler for any events that can be generated by the widgets you add here (sliders, radio, checkbox, etc)
BEGIN_EVENT_TABLE(RipPlannerTab, wxPanel)
EVT_COMMAND (wxID_ANY, wxEVT_GRIP_SLIDER_CHANGE, RipPlannerTab::OnSlider)
EVT_COMMAND (wxID_ANY, wxEVT_COMMAND_RADIOBOX_SELECTED, RipPlannerTab::OnRadio)
EVT_COMMAND (wxID_ANY, wxEVT_COMMAND_BUTTON_CLICKED, RipPlannerTab::OnButton)
EVT_COMMAND (wxID_ANY, wxEVT_COMMAND_CHECKBOX_CLICKED, RipPlannerTab::OnCheckBox)
END_EVENT_TABLE()

// Class constructor for the tab: Each tab will be a subclass of RSTTab
IMPLEMENT_DYNAMIC_CLASS(RipPlannerTab, GRIPTab)

/**
 * @function RipTabPlanner
 * @brief Constructor
 */
RipPlannerTab::RipPlannerTab( wxWindow *parent, const wxWindowID id,
		              const wxPoint& pos, const wxSize& size, long style) :
	                      GRIPTab(parent, id, pos, size, style) {

    startConf.resize(0);
    goalConf.resize(0);

    robotId = 0;
    links.resize(0);

    rrtStyle = 0;
    greedyMode = false;
    connectMode = false;
    smooth = false;
    planner = NULL;

    sizerFull = new wxBoxSizer( wxHORIZONTAL );
 
    // ** Create left static box for configuring the planner **

    // Create StaticBox container for all items
    wxStaticBox* configureBox = new wxStaticBox(this, -1, wxT("Configure"));

    // Create sizer for this box with horizontal layout
    wxStaticBoxSizer* configureBoxSizer = new wxStaticBoxSizer(configureBox, wxHORIZONTAL);

    // Create a sizer for radio buttons in 1st column
    wxBoxSizer *col1Sizer = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer *miniSizer = new wxBoxSizer(wxVERTICAL); // annoying hack to get checkboxes close together
    miniSizer->Add( new wxCheckBox(this, checkbox_beGreedy, _T("&goal bias (be greedy)")),
		    1, // vertical stretch evenly
		    wxALIGN_NOT,
		    0);
    miniSizer->Add( new wxCheckBox(this, checkbox_useConnect, _T("use &connect algorithm (be really greedy)")),
		    1, // vertical stretch evenly
		    wxALIGN_NOT,
		    0 );
    miniSizer->Add( new wxCheckBox(this, checkbox_useSmooth, _T("use &smoother (make it less ugly)")),
		    1, // vertical stretch evenly
		    wxALIGN_NOT,
		    0 );
    col1Sizer->Add(miniSizer,1,wxALIGN_NOT,0);

    // Create radio button for rrt_style
    static const wxString RRTStyles[] =
    {
        wxT("Single"),
	wxT("Bi-directional")
    };
    col1Sizer->Add( new wxRadioBox(this, wxID_ANY, wxT("RRT &style:"),
		    wxDefaultPosition, wxDefaultSize, WXSIZEOF(RRTStyles), RRTStyles, 1,
		    wxRA_SPECIFY_ROWS),
		    1, // stretch evenly with buttons and checkboxes
		    wxALIGN_NOT,
		    0 );
    // Add col1 to configureBoxSizer
    configureBoxSizer->Add( col1Sizer,
			    3, // 3/5 of configure box
			    wxALIGN_NOT,
			    0 ); //

    // Create sizer for start buttons in 2nd column
    wxBoxSizer *col2Sizer = new wxBoxSizer(wxVERTICAL);
    col2Sizer->Add( new wxButton(this, button_SetStart, wxT("Set &Start")),
		    0, // make horizontally unstretchable
		    wxALL, // make border all around (implicit top alignment)
		    1 ); // set border width to 1, so start buttons are close together
    col2Sizer->Add( new wxButton(this, button_showStart, wxT("Show S&tart")),
		    0, // make horizontally unstretchable
		    wxALL, // make border all around (implicit top alignment)
		    1 ); // set border width to 1, so start buttons are close together
    col2Sizer->Add( new wxButton(this, button_empty1, wxT("Empty 1")),
		    0, // make horizontally unstretchable
		    wxALL, // make border all around (implicit top alignment)
		    1 ); // set border width to 1, so start buttons are close together


    // Add col2Sizer to the configuration box
    configureBoxSizer->Add( col2Sizer,
			    1, // takes half the space of the configure box
			    wxALIGN_NOT ); // no border and center horizontally

    // Create sizer for goal buttons in 3rd column
    wxBoxSizer *col3Sizer = new wxBoxSizer(wxVERTICAL);
    col3Sizer->Add( new wxButton(this, button_SetGoal, wxT("Set &Goal")),
		    0, // make horizontally unstretchable
		    wxALL, // make border all around (implicit top alignment)
		    1 ); // set border width to 1, so start buttons are close together
    col3Sizer->Add( new wxButton(this, button_showGoal, wxT("Show G&oal")),
		    0, // make horizontally unstretchable
		    wxALL, // make border all around (implicit top alignment)
		    1 ); // set border width to 1, so start buttons are close together
    col3Sizer->Add( new wxButton(this, button_empty2, wxT("Empty 2")),
		    0, // make horizontally unstretchable
		    wxALL, // make border all around (implicit top alignment)
		    1 ); // set border width to 1, so start buttons are close together
    configureBoxSizer->Add( col3Sizer,
			    1, // size evenly with radio box and checkboxes
			    wxALIGN_NOT ); // no border and center horizontally

    // Add this box to parent sizer
    sizerFull->Add( configureBoxSizer,
		    4, // 4-to-1 ratio with execute sizer, since it just has 3 buttons
		    wxEXPAND | wxALL,
		    6 );


    // ** Create right static box for running the planner **
    wxStaticBox* executeBox = new wxStaticBox(this, -1, wxT("Execute Planner"));

    // Create sizer for this box
    wxStaticBoxSizer* executeBoxSizer = new wxStaticBoxSizer(executeBox, wxVERTICAL);

    // Add buttons for "plan", "save movie", and "show path"
    executeBoxSizer->Add( new wxButton(this, button_Plan, wxT("&Start")),
	 		  1, // stretch to fit horizontally
			  wxGROW ); // let it hog all the space in it's column

    executeBoxSizer->Add( new wxButton(this, button_Stop, wxT("&Stop")),
			  1, // stretch to fit horizontally
			  wxGROW );


    wxBoxSizer *timeSizer = new wxBoxSizer(wxHORIZONTAL);
    timeText = new wxTextCtrl(this,1008,wxT("5.0"),wxDefaultPosition,wxSize(40,20),wxTE_RIGHT);//,wxTE_PROCESS_ENTER | wxTE_RIGHT);
    timeSizer->Add(timeText,2,wxALL,1);
    timeSizer->Add(new wxButton(this, button_UpdateTime, wxT("Set T(s)")),2,wxALL,1);
    executeBoxSizer->Add(timeSizer,1,wxALL,2);

    executeBoxSizer->Add( new wxButton(this, button_ShowPath, wxT("&Print")),
			  1, // stretch to fit horizontally
			  wxGROW );

    sizerFull->Add(executeBoxSizer, 1, wxEXPAND | wxALL, 6);

    SetSizer(sizerFull);

}

/**
 * @function OnRadio
 * @brief Handle Radio toggle
 */
void RipPlannerTab::OnRadio(wxCommandEvent &evt) {
    // ====================== YOUR CODE HERE =========================
    // INSTRUCTIONS: Add code here to implement a bi-directional RRT

    // ================================================================
	rrtStyle = evt.GetSelection();
	cout << "rrtStyle = " << rrtStyle << endl;
}

/**
 * @function OnButton
 * @brief Handle Button Events
 */
void RipPlannerTab::OnButton(wxCommandEvent &evt) {

    int button_num = evt.GetId();

    switch (button_num) {

        /** Set Start */
        case button_SetStart:
	    if ( mWorld != NULL ) {
	        if( mWorld->mRobots.size() < 1) {
            	    cout << "--(!) Must have a world with a robot to set a Start state (!)--" << endl;
		    break;
		}
		cout << "--(i) Setting Start state for " << mWorld->mRobots[robotId]->getName() << ":" << endl;

                startConf = mWorld->mRobots[robotId]->getQuickDofs();

		for( unsigned int i = 0; i < startConf.size(); i++ )
                {  cout << startConf(i) << " ";  } 
		cout << endl;
	    } else {
	        cout << "--(!) Must have a world loaded to set a Start state.(!)--" << endl;
	    }
	    break;

        /** Set Goal */
	case button_SetGoal:
	    if ( mWorld != NULL ) {
	        if( mWorld->mRobots.size() < 1){
		cout << "--(!) Must have a world with a robot to set a Goal state.(!)--" << endl;
		break;
		}
		cout << "--(i) Setting Goal state for " << mWorld->mRobots[robotId]->getName() << ":" << endl;

                goalConf = mWorld->mRobots[robotId]->getQuickDofs();

		for( unsigned int i = 0; i < goalConf.size(); i++ )
                {  cout << goalConf(i) << " "; } 
		cout << endl;
	    } else {
	        cout << "--(!) Must have a world loaded to set a Goal state (!)--" << endl;
	    }
	    break;

        /** Show Start */
	case button_showStart:
	    if( startConf.size() < 1 ){
	        cout << "--(x) First, set a start configuration (x)--" << endl;
		break;
	    } 

            mWorld->mRobots[robotId]->setQuickDofs( startConf );

	    for( unsigned int i = 0; i< startConf.size(); i++ )
            {  cout << startConf(i) << " "; }
	    cout << endl;

	    mWorld->mRobots[robotId]->update();
	    viewer->UpdateCamera(); 
	    break;

        /** Show Goal */
	case button_showGoal:
	    if(goalConf.size() < 1){
	        cout << "--(x) First, set a goal configuration (x)--" << endl;
		break;
	    }

            mWorld->mRobots[robotId]->setQuickDofs( goalConf );

	    for( unsigned int i = 0; i< goalConf.size(); i++ )
            {  cout << goalConf[i] << " ";  }
	    cout << endl;

	    mWorld->mRobots[robotId]->update();
	    viewer->UpdateCamera(); 
	    break;

        /** Reset Planner */ 
	case button_resetPlanner:
	    if ( mWorld != NULL) {
	        if ( planner != NULL)
		    delete planner;
               
		cout << "Creating a new planner" << endl;
                double stepSize = 0.1; // default
		planner = new PathPlanner( *mWorld, false, stepSize );
	    } else {
	        cout << "--(!) Must have a world loaded to make a planner (!)--" << endl;
	    }
	    break;

        /** Empty button 1 */
	case button_empty1:
     {
	    cout << "-- (0) Checking Collisions (0)--" << endl;

      //mCollision->UpdateAllCollisionModels();
      bool st = mCollision->CheckCollisions();
      if( st == true )
      { printf("Collisions \n");}
      else
      { printf("No Collisions \n");}

     }
	    break;

        /** Empty button 2 */
	case button_empty2:
	    cout << "-- (0) Empty Button to use for whatever you want (0)--" << endl;
	    break;

        /** Execute Plan */
	case button_Plan:
           {         
	    if( goalConf.size() < 0 ){ cout << "--(x) Must set a goal (x)--" << endl; break; }
	    if( startConf.size() < 0 ){ cout << "--(x) Must set a start (x)--" << endl; break; }
	    if( mWorld == NULL ){ cout << "--(x) Must load a world (x)--" << endl; break; }
	    if( mWorld->mRobots.size() < 1){ cout << "--(x) Must load a world with a robot(x)--" << endl; break; }

            double stepSize = 0.02;

	    planner = new PathPlanner( *mWorld, mCollision, false, stepSize );
	    //wxThread planThread;
	    //planThread.Create();
            links = mWorld->mRobots[robotId]->getQuickDofsIndices();

            int maxNodes = 5000;
	    bool result = planner->planPath( robotId, 
                                             links, 
                                             startConf, 
                                             goalConf, 
                                             rrtStyle,  
                                             connectMode,
                                             greedyMode, 
                                             smooth, 
                                             maxNodes );
            if( result  )
            {  SetTimeline(); }
           }
	    break;

	case button_UpdateTime:
           {
	    /// Update the time span of the movie timeline
	    SetTimeline();
           }		
	    break;

        /** Show Path */
	case button_ShowPath:            
	    if( mWorld == NULL || planner == NULL || planner->path.size() == 0 ) {
	        cout << "--(!) Must create a valid plan before printing. (!)--" << endl;
		return;
	    } else {
                printf("--(i) Printing (i)-- \n");
            }        
	    break;
	}
}

/**
 * @function setTimeLine
 * @brief 
 */
void RipPlannerTab::SetTimeline() {

    if( mWorld == NULL || planner == NULL || planner->path.size() == 0 ) {
        cout << "--(!) Must create a valid plan before updating its duration (!)--" << endl;
	return;
    }

    double T;
    timeText->GetValue().ToDouble(&T);

    int numsteps = planner->path.size();
    double increment = T/(double)numsteps;

    cout << "-->(+) Updating Timeline - Increment: " << increment << " Total T: " << T << " Steps: " << numsteps << endl;

    frame->InitTimer( string("RRT_Plan"),increment );

    Eigen::VectorXd vals( links.size() );

    for( std::list<Eigen::VectorXd>::iterator it = planner->path.begin(); it != planner->path.end(); it++ ) {

        mWorld->mRobots[robotId]->setQuickDofs( *it );
	mWorld->mRobots[robotId]->update();

        frame->AddWorld( mWorld );
    }

}

/**
 * @function OnCheckBox
 * @brief Handle CheckBox Events
 */ 
void RipPlannerTab::OnCheckBox( wxCommandEvent &evt ) {
    int checkbox_num = evt.GetId();

    switch (checkbox_num) {
        
        case checkbox_beGreedy:
            greedyMode = (bool)evt.GetSelection();
	    cout << "--> greedy = " << greedyMode << endl;
	    break;

	case checkbox_useConnect:
	    connectMode = (bool)evt.GetSelection();
	    cout << "--> useConnect = " << connectMode << endl;
	    break;
	case checkbox_useSmooth:
	    smooth = (bool)evt.GetSelection();
	    cout << "--> Smooth option = " << smooth << endl;
	    break;
    }
}

/**
 * @function OnSlider
 * @brief Handle slider changes
 */
void RipPlannerTab::OnSlider(wxCommandEvent &evt) {

    if (selectedTreeNode == NULL) {
        return;
    }

    int slnum = evt.GetId();
    double pos = *(double*) evt.GetClientData();
    char numBuf[1000];

    switch (slnum) {
        case slider_Time:
	    sprintf(numBuf, "X Change: %7.4f", pos);
	    cout << "-->(i) Timeline slider output: " << numBuf << endl;
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

/**
 * @function GRIPStateChange
 * @brief This function is called when an object is selected in the Tree View or other
 *        global changes to the RST world. Use this to capture events from outside the tab.
 */
void RipPlannerTab::GRIPStateChange() {
    if ( selectedTreeNode == NULL ) {
        return;
    }

    string statusBuf;
    string buf, buf2;

    switch (selectedTreeNode->dType) {

        case Return_Type_Object:
	    selectedObject = (planning::Object*) ( selectedTreeNode->data );
	    statusBuf = " Selected Object: " + selectedObject->getName();
	    buf = "You clicked on object: " + selectedObject->getName();

	    // Enter action for object select events here:

	    break;
	case Return_Type_Robot:
	    selectedRobot = (planning::Robot*) ( selectedTreeNode->data );
	    statusBuf = " Selected Robot: " + selectedRobot->getName();
	    buf = " You clicked on robot: " + selectedRobot->getName();

	    // Enter action for Robot select events here:

	    break;
	case Return_Type_Node:
	    selectedNode = (kinematics::BodyNode*) ( selectedTreeNode->data );
	    statusBuf = " Selected Body Node: " + string(selectedNode->getName()) + " of Robot: "
			+ ( (planning::Robot*) selectedNode->getSkel() )->getName();
	    buf = " Node: " + string(selectedNode->getName()) + " of Robot: " + ( (planning::Robot*) selectedNode->getSkel() )->getName();

	    // Enter action for link select events here:

	    break;
        default:
            fprintf(stderr, "--( :D ) Someone else's problem!\n");
            assert(0);
            exit(1);
    }

    //cout << buf << endl;
    frame->SetStatusText(wxString(statusBuf.c_str(), wxConvUTF8));
    sizerFull->Layout();
}
