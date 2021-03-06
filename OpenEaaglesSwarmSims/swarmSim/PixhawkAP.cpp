#include "PixhawkAP.h"
#include "UAV.h"

#include "openeaagles/basic/Number.h"
#include "openeaagles/basic/Thread.h"
#include "openeaagles/basic/Nav.h"
#include "openeaagles/basic/osg/Vec3"
#include "openeaagles/basic/EarthModel.h"
#include "openeaagles/simulation/Player.h"
#include "openeaagles/simulation/Navigation.h"
#include "openeaagles/simulation/Steerpoint.h"
#include "openeaagles/simulation/Route.h"
#include "openeaagles/simulation/Simulation.h"
#include "openeaagles/dynamics/JSBSimModel.h"
#include "JSBSim/FGFDMExec.h"
#include "JSBSim/models/FGFCS.h"
#include "JSBSim/models/FGAtmosphere.h"
#include "JSBSim/models/FGAuxiliary.h"

#include "coremag.hxx"
#include "ctime"

// used for testing
#include <iostream>
#include <iomanip>
#include <thread>
#include <conio.h>

#define PI 3.1415926535897932384626433832795

namespace Eaagles {
namespace Swarms {

// =============================================================================
// class: PixhawkAP
// =============================================================================

IMPLEMENT_SUBCLASS(PixhawkAP,"PixhawkAP")

BEGIN_SLOTTABLE(PixhawkAP)
	"portNum",
	"mode",
	"statustexts",
END_SLOTTABLE(PixhawkAP)

BEGIN_SLOT_MAP(PixhawkAP)
	ON_SLOT(1, setSlotPortNum,     Basic::Number) // 1) USB COM port assigned to specified PX4
	ON_SLOT(2, setSlotMode,        Basic::String) // 2) modes: "nav" and "swarm" (autonomous flight)
	                                              //    default = "nav"
	ON_SLOT(3, setSlotStatustexts, Basic::String) // 3) Statustexts refers to printing to console the contents
	                                              //    of mavlink STATUSTEXT (#253) message received from PX4
												  //    default = "off"
END_SLOT_MAP()                                                                        

Eaagles::Basic::Object* PixhawkAP::getSlotByIndex(const int si) {                                                                                     
	return BaseClass::getSlotByIndex(si);
}

EMPTY_SERIALIZER(PixhawkAP)

//------------------------------------------------------------------------------------
// Constructor
//------------------------------------------------------------------------------------
PixhawkAP::PixhawkAP() {
	STANDARD_CONSTRUCTOR()

	sentInitCmd      = false;

	hil_gps_time     = 0;
	hil_sensor_time  = 0;
	heartbeat_time   = 0;
	set_mode_time    = 5000000; // wait 5 seconds before checking mode
	mag_time         = 0;
	startTimeSet     = false;

	sid              = 255;
	cid              = 0;
	hbSid			 = 0;
	hbCid			 = 0;
	hbType			 = 0;
	hbAutopilot		 = 0;
	hbBaseMode		 = 0;
	hbCustomMode	 = 0;
	hbSystemStatus	 = 0;
	hbMavlinkVersion = 0;
	miLat            = 0;
	miLon            = 0;
	miAlt            = 0;
	mcCnt            = 0;
	hcRollCtrl	     = 0.0;
	hcPitchCtrl	     = 0.0;
	hcYawCtrl	     = 0.0;
	hcThrottleCtrl   = 0.8;
	hcSysMode    	 = 0;
	hcNavMode    	 = 0;
	dwLat            = 0;
	dwLon            = 0;
	dwAlt            = 0;
	dwLLA.set(0, 0, 0);
	msnCntSent       = false;
	msnReqRcvd       = false;
	msnItmSent       = false;
	msnAckRcvd       = false;
	msnTimeout       = 0;
	msnTimeoutCount  = 0;
	currState        = SEND_COUNT;
	newWaypointSet   = 0;
	portNum          = 0;
	mode = nullptr;
	setMode(new Basic::String("nav"));
	statustexts = nullptr;
	setStatustexts(new Basic::String("off"));

	messageIDStartTime = getComputerTime();
	messageIDsIndex = 0;
	printedMessageIDs = false;

	// buffer variables
	bufferSize = 128;
	bytesRead = 0;                          // index for lpBuffer
	lpBuffer = new char[bufferSize];        // holds incoming serial data

	for (int i = 0; i < 256; i++) {
		mavRcvd[i] = 0;
	}
}

//------------------------------------------------------------------------------------
// copyData() - copies one object to another
//------------------------------------------------------------------------------------
void PixhawkAP::copyData(const PixhawkAP& org, const bool cc) {
	BaseClass::copyData(org);

	sentInitCmd      = org.sentInitCmd;

	hil_gps_time     = org.hil_gps_time;
	hil_sensor_time  = org.hil_sensor_time;
	heartbeat_time   = org.heartbeat_time;
	set_mode_time    = org.set_mode_time;
	mag_time         = org.mag_time;
	startTimeSet     = org.startTimeSet;

	sid              = org.sid;
	cid              = org.cid;
	hbSid			 = org.hbSid;
	hbCid			 = org.hbCid;
	hbType			 = org.hbType;
	hbAutopilot		 = org.hbAutopilot;
	hbBaseMode		 = org.hbBaseMode;
	hbCustomMode	 = org.hbCustomMode;
	hbSystemStatus	 = org.hbSystemStatus;
	hbMavlinkVersion = org.hbMavlinkVersion;
	miLat            = org.miLat;
	miLon            = org.miLon;
	miAlt            = org.miAlt;
	mcCnt            = org.mcCnt;
	hcRollCtrl	     = org.hcRollCtrl;
	hcPitchCtrl	     = org.hcPitchCtrl;
	hcYawCtrl	     = org.hcYawCtrl;
	hcThrottleCtrl   = org.hcThrottleCtrl;
	hcSysMode    	 = org.hcSysMode;
	hcNavMode    	 = org.hcNavMode;
	portNum          = org.portNum;
	dwLat            = org.dwLat;
	dwLon            = org.dwLon;
	dwAlt            = org.dwAlt;
	dwLLA            = org.dwLLA;
	msnCntSent       = org.msnCntSent;
	msnReqRcvd       = org.msnReqRcvd;
	msnItmSent       = org.msnItmSent;
	msnAckRcvd       = org.msnAckRcvd;
	msnTimeout       = org.msnTimeout;
	msnTimeoutCount  = org.msnTimeoutCount;
	currState        = SEND_COUNT;
	newWaypointSet   = org.newWaypointSet;

	if(cc) {
		mode = nullptr;
		statustexts = nullptr;
	}

	Basic::String* m = nullptr;
	if (org.mode != nullptr) m = org.mode->clone();
	mode = m;
	if (m != 0) m->unref();

	Basic::String* s = nullptr;
	if (org.statustexts != nullptr) m = org.statustexts->clone();
	statustexts = s;
	if (s != 0) s->unref();

	bufferSize = org.bufferSize;
	bytesRead = 0;                          // index for lpBuffer
	lpBuffer = new char[bufferSize];        // holds incoming serial data
}

//------------------------------------------------------------------------------------
// deleteData() -- delete this object's data
//------------------------------------------------------------------------------------
void PixhawkAP::deleteData() {
	serial.Close();
	mode = nullptr;
	statustexts = nullptr;
	delete[] lpBuffer;
}

//------------------------------------------------------------------------------------
// Rotation - rotate a coordinate around the origin by roll(phi), pitch(theta), and yaw(psi) angles
//------------------------------------------------------------------------------------

double* PixhawkAP::rollPitchYaw(double x, double y, double z, bool inDegrees, bool reverse, double phi, double theta, double psi) {
	// convert degrees to radians
	if (inDegrees) {
		phi   *= PI / 180;
		theta *= PI / 180;
		psi   *= PI / 180;
	}

	// reverse the Euler angles
	if (reverse) {
		phi   = -phi;
		theta = -theta;
		psi   = -psi;
	}

	double Rx[3][3] = { { 1, 0, 0 }, { 0, cos(phi), -sin(phi) }, { 0, sin(phi), cos(phi) } };
	double Ry[3][3] = { { cos(theta), 0, sin(theta) }, { 0, 1, 0 }, { -sin(theta), 0, cos(theta) } };
	double Rz[3][3] = { { cos(psi), -sin(psi), 0 }, { sin(psi), cos(psi), 0 }, { 0, 0, 1 } };

	double S0[3] = { x, y, z };

	double S1[3] = { 0, 0, 0 };
	for (int i = 0; i < 3; i++)
		for (int j = 0; j < 3; j++)
			if (reverse)
				S1[i] += Rz[i][j] * S0[j]; // Yaw
			else
				S1[i] += Rx[i][j] * S0[j]; // Roll

	double S2[3] = { 0, 0, 0 };
	for (int i = 0; i < 3; i++)
		for (int j = 0; j < 3; j++)
			S2[i] += Ry[i][j] * S1[j]; // Pitch

	double S3[3] = { 0, 0, 0 };
	for (int i = 0; i < 3; i++)
		for (int j = 0; j < 3; j++)
			if (reverse)
				S3[i] += Rx[i][j] * S2[j]; // Roll
			else
				S3[i] += Rz[i][j] * S2[j]; // Yaw

	return S3;
}

//------------------------------------------------------------------------------------
// Records Mavlink messages (ID and timestamp)
//------------------------------------------------------------------------------------

void PixhawkAP::recordMessage(uint8_t msgid, bool sending, int byteCnt) {
	return;
	recordMutex.lock();
	if (!printedMessageIDs) {
		if (messageIDsIndex < 10000) {
			messageTSs[messageIDsIndex] = (int)((getComputerTime() - messageIDStartTime) * 1000); // timestamp
			messageIDs[messageIDsIndex] = msgid; // msg id
			messageDRs[messageIDsIndex] = sending; // sending/receiving
			messageBCs[messageIDsIndex] = byteCnt;
			messageIDsIndex++;
		} else {
			cout << "MAVLINK MESSAGES RECORDED TO FILE" << endl;
			ofstream output;
			char filename[25];
			sprintf(filename, "COM%d_messageIDs.csv", portNum);
			output.open(filename, ios::trunc);
			if (output.is_open()) {
				// print messages
				for (int i = 0; i < messageIDsIndex; i++) {
					if (messageDRs[i]) {
						output << "COM" << portNum << ",SND," << messageTSs[i] << "," << (int)messageIDs[i] << "," << messageBCs[i] << "\n";
					} else {
						output << "COM" << portNum << ",RCV," << messageTSs[i] << "," << (int)messageIDs[i] << "," << messageBCs[i] << "\n";
					}
				}
				printedMessageIDs = true;
				cout << "Message traffic successfully saved to '" << filename << "'" << endl;
				output.close();
			}
		}
	}
	recordMutex.unlock();
}

//------------------------------------------------------------------------------
// Getter methods
//------------------------------------------------------------------------------

const char* PixhawkAP::getMode() const {
	const char* p = 0;
	if (mode != 0) p = *mode;
	return p;
}

const char* PixhawkAP::getStatustexts() const {
	const char* p = 0;
	if (statustexts != 0) p = *statustexts;
	return p;
}

//------------------------------------------------------------------------------------
// Setter methods
//------------------------------------------------------------------------------------

// used by OCA to update Dynamic Waypoint
void PixhawkAP::setWaypoint(const osg::Vec3& posNED, const LCreal altMeters) {
	if (strcmp(*mode, "swarm") != 0) return;
	dwLLA.set(posNED.x(), posNED.y(), posNED.z());

	Simulation::Simulation* s = getOwnship()->getSimulation();
	const double maxRefRange = s->getMaxRefRange();
	const Basic::EarthModel* em = s->getEarthModel();

	// Compute & set the lat/lon/alt position
	const double refLat = s->getRefLatitude();
	const double refLon = s->getRefLongitude();
	const double cosRlat = s->getCosRefLat();

	if (s->isGamingAreaUsingEarthModel()) {
		const double sinRlat = s->getSinRefLat();
		Basic::Nav::convertPosVec2llE(refLat, refLon, sinRlat, cosRlat, dwLLA, &dwLat, &dwLon, &dwAlt, em);
	} else {
		Basic::Nav::convertPosVec2llS(refLat, refLon, cosRlat, dwLLA, &dwLat, &dwLon, &dwAlt);
	}

	dwAlt = altMeters;
	newWaypointSet = 0;
}

//------------------------------------------------------------------------------------
// Slot methods
//------------------------------------------------------------------------------------

bool PixhawkAP::setSlotPortNum(const Basic::Number* const msg) {
	bool ok = (msg != nullptr);
	if (ok) setPortNum(msg->getInt());
	return ok;
}

bool PixhawkAP::setSlotMode(const Basic::String* const msg) {
	bool ok = false;
	if (msg != nullptr) {
		if (strcmp(msg->getString(), "nav") == 0) { setMode(msg); }
		else setMode(new Basic::String("swarm"));
		ok = true;
	}
	return ok;
}

bool PixhawkAP::setSlotStatustexts(const Basic::String* const msg) {
	bool ok = false;
	if (msg != nullptr) {
		if (strcmp(msg->getString(), "on") == 0) { setStatustexts(msg); }
		else setStatustexts(new Basic::String("off"));
		ok = true;
	}
	return ok;
}

//------------------------------------------------------------------------------
// Tracks how long this program has been running
//------------------------------------------------------------------------------

// time (in usec) since program started
uint64_t PixhawkAP::usecSinceSystemBoot() {
	if (!startTimeSet) {
		startTimeSet = true;
		startTime = std::chrono::high_resolution_clock::now(); // set start time of program execution
	}
	return chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - startTime).count();
}

//------------------------------------------------------------------------------
// Send UAV simulation state (i.e. UAV position/attitude/velocity/etc.) to PX4
//------------------------------------------------------------------------------

// HEARTBEAT (0) at 1 Hz
void PixhawkAP::sendHeartbeat() {
	mavlink_msg_heartbeat_pack(sid, cid, &msg1, 6, 8, 192, 0, 4);
	sendMessage(&msg1);
}

// HIL_SENSOR (107) at 50 Hz
void PixhawkAP::sendHilSensor() {
	UAV* uav = dynamic_cast<UAV*>(getOwnship());
	if (uav == nullptr) return;

	Eaagles::Dynamics::JSBSimModel* dm = dynamic_cast<Eaagles::Dynamics::JSBSimModel*>(uav->getDynamicsModel());
	if (dm == nullptr) return;

	float xacc = uav->getAcceleration().x();
	float yacc = uav->getAcceleration().y();
	float zacc = uav->getAcceleration().z() - 9.68235;
	//cout << "\racc:" << xacc << "\t" << yacc << "\t" << zacc << "                          ";

	float xgyro = uav->getAngularVelocities().x();
	float ygyro = uav->getAngularVelocities().y();
	float zgyro = uav->getAngularVelocities().z();
	//cout << "\rgyro:" << xgyro << "\t" << ygyro << "\t" << zgyro << "                          ";

	double phiR   = uav->getRollR();
	double thetaR = uav->getPitchR();
	double psiR   = uav->getHeadingR();
	//cout << "roll: " << phiR << " | pitch: " << thetaR << " | yaw: " << psiR << endl;

	// Static north pointing unit vector of magnetic field in the Inertial (NED) Frame 15K ft MSL
	// over USAFA Airfield on 12/29/2015 (dec/inc of 8.3085/65.7404 degs respectively)
	xmag = 0.902124413;
	ymag = 0.131742403;
	zmag = 0.410871614;

	// Rotate vector
	double* reverseRotation = rollPitchYaw(xmag, ymag, zmag, false, true, phiR, thetaR, psiR);
	xmag = reverseRotation[0];
	ymag = reverseRotation[1];
	zmag = reverseRotation[2];
	//cout << "\rrollCtrl: " << hcRollCtrl << " | pitchCtrl: " << hcPitchCtrl << " | yawCtrl: " << hcYawCtrl << "                                  "; //###
	//cout << "\rmag:" << xmag << "\t" << ymag << "\t" << zmag << "                          ";

	JSBSim::FGFDMExec* fdmex = dm->getJSBSim();                if (fdmex == nullptr) return;      // get JSBSim
	JSBSim::FGAtmosphere* Atmosphere = fdmex->GetAtmosphere(); if (Atmosphere == nullptr) return; // get atmosphere from JSBSim
	JSBSim::FGAuxiliary* Auxiliary = fdmex->GetAuxiliary();    if (Auxiliary == nullptr) return;  // get auxiliary from JSBSim

	// get absolute pressure (in millibars)
	float abs_pressure = static_cast<LCreal>(Atmosphere->GetPressure()) * 68.9475728 / 144;

	// get pressure altitude
	double altFt = static_cast<LCreal>(Atmosphere->GetPressureAltitude()); // in feet
	float pressure_alt = altFt / 3.28083989502;                            // convert to meters

	// get differential pressure (in millibars)
	double density = Atmosphere->GetDensity(altFt) * 515.379;             // convert from slug/ft^3 to kg/m^3
	double velocity = Auxiliary->GetVtrueKTS() * 0.514444444;             // convert from Kts to m/s
	float diff_pressure = ((density * velocity * velocity * 0.5) * 0.01); // convert from pascal to millibar

	// get temperature (in Celsius)
	float temperature = (static_cast<LCreal>(Atmosphere->GetTemperature()) - 491.67) / 1.8; // convert from Rankine to Celsius using 

	//cout << "\rabs_pressure: " << abs_pressure << "\tdiff_pressure: " << diff_pressure << "\tpressure_alt: " << pressure_alt << "\ttemperature: " << temperature << "                          ";

	// send HIL_SENSOR message
	mavlink_msg_hil_sensor_pack(sid, cid, &msg2, usecSinceSystemBoot(),
		xacc,
		yacc,
		zacc,
		xgyro,
		ygyro,
		zgyro,
		xmag,
		ymag,
		zmag,
		abs_pressure,
		diff_pressure,
		pressure_alt,
		temperature,
		4095);

	sendMessage(&msg2);
}

void PixhawkAP::updateMagValues() {
	UAV* uav = dynamic_cast<UAV*>(getOwnship());
	if (uav == nullptr) return;

	// Static north pointing unit vector of magnetic field in the Inertial (NED) Frame 15K ft MSL
	// over USAFA Airfield on 12/29/2015 (dec/inc of 8.3085/65.7404 degs respectively)
	//float xmag = 0.902124413;
	//float ymag = 0.131742403;
	//float zmag = 0.410871614;

	//float xmag = 0.860429164;
	//float ymag = 0.054813282;
	//float zmag = 0.383757034;

	// get day, mth, yr
	time_t t = time(NULL);
	tm* timePtr = localtime(&t);
	int yy, mm, dd;
	yy = timePtr->tm_year - 100;
	mm = timePtr->tm_mon + 1;
	dd = timePtr->tm_mday;

	// get mag
	double field[6];
	double latR = uav->getLatitude()*PI / 180;
	double lonR = uav->getLongitude()*PI / 180;
	double altKm = uav->getAltitudeM() / 1000;
	calc_magvar(latR, lonR, altKm, yymmdd_to_julian_days(yy, mm, dd), field);
	double magnitude = sqrt(field[3] * field[3] + field[4] * field[4] + field[5] * field[5]);

	// set magvar
	xmag = field[5] / magnitude;
	ymag = field[4] / magnitude;
	zmag = field[3] / magnitude;
}

// HIL_GPS (113) at 10 Hz
void PixhawkAP::sendHilGps() {
	UAV* uav = dynamic_cast<UAV*>(getOwnship());
	if (uav == nullptr) return;

	Eaagles::Dynamics::JSBSimModel* dm = dynamic_cast<Eaagles::Dynamics::JSBSimModel*>(uav->getDynamicsModel());
	if (dm == nullptr) return;

	uint8_t  fix_type = 3;
	double dlat, dlon, dalt;
	uav->getPositionLLA(&dlat, &dlon, &dalt);
	int32_t  lat = (int32_t)(dlat * 10000000);
	int32_t  lon = (int32_t)(dlon * 10000000);
	int32_t  alt = (int32_t)(dalt * 1000);
	uint16_t eph = 30;
	uint16_t epv = 60;
	uint16_t vel = uav->getGroundSpeed() * 100;
	int16_t  vn = uav->getVelocity().x() * 100;
	int16_t  ve = uav->getVelocity().y() * 100;
	int16_t  vd = uav->getVelocity().z() * 100;

	double   cog_signed = uav->getGroundTrackD();
	if (cog_signed < 0) cog_signed = 360 + cog_signed; // convert from -180 thru 180 to 0 thru 360
	uint16_t cog = cog_signed * 100;

	uint8_t  satellites_visible = 8;

	mavlink_msg_hil_gps_pack(sid, cid, &msg2, usecSinceSystemBoot(),
		fix_type,
		lat,
		lon,
		alt,
		eph,
		epv,
		vel,
		vn,
		ve,
		vd,
		cog,
		satellites_visible);

	sendMessage(&msg2);
}

// Dynamic Waypoint Following (DWF)
void PixhawkAP::sendDynamicWaypoint() {
	UAV* uav = dynamic_cast<UAV*>(getOwnship());
	if (uav == nullptr) return;

	/*
	 ___________________________________________________
	| Sequence Diagram for waypoint updates:            |
	|___________________________________________________|
	|           SIM          |          PX4             |
	|________________________|__________________________|
	|                        |                          |
	| MISSION_COUNT (44) --> |                          |
	|                        |                          |
	|                        | <-- MISSION_REQUEST (40) |
	|                        |                          |
	|  MISSION_ITEM (39) --> |                          |
	|                        |                          |
	|                        | <-- MISSION_ACK (47)     |
	|________________________|__________________________|

	*/

	switch (currState) {
	case SEND_COUNT:
		currState = AWAIT_REQ;
		msnReqRcvd = false;
		msnTimeout = usecSinceSystemBoot() + 250000; // wait 250 ms before trying again
		msnTimeoutCount = 0;
		// send MISSION_COUNT (44)
		mavlink_msg_mission_count_pack(sid, cid, &msg3, 1, 190, 1);
		sendMessage(&msg3);
		break;
	case AWAIT_REQ:
		if (msnReqRcvd) {
			currState = SEND_ITEM;
		} else if (msnTimeout < usecSinceSystemBoot()) {
			currState = SEND_COUNT;
		}
		break;
	case SEND_ITEM:
		currState = AWAIT_ACK;
		msnAckRcvd = false;
		msnTimeout = usecSinceSystemBoot() + 250000; // wait 250 ms sec before trying again
		// check for RTL mode
		if (waypointTooFar) {
			Swarms::UAV* uav = dynamic_cast<Swarms::UAV*>(this->getOwnship());
			if (uav == 0) return;
			uav->getPositionLLA(&dwLat, &dwLon, &dwAlt);
			waypointTooFar = (newWaypointSet == 0);
		}
		// send MISSION_ITEM (39)
		//cout << "\rlat: " << dwLat << "\tlon: " << dwLon << "\talt: " << dwAlt << "                                            ";
		mavlink_msg_mission_item_pack(sid, cid, &msg3, 1, 190, 0, 0, 16, 1, 1, 0, 25, 0, 0, dwLat, dwLon, dwAlt);
		sendMessage(&msg3);
		break;
	case AWAIT_ACK:
		if (msnAckRcvd) {
			currState = SEND_COUNT;
			newWaypointSet++;
		} else if (msnTimeout < usecSinceSystemBoot()) {
			currState = SEND_COUNT;
		}
		break;
	}
}

// Controls timing of each MAVLink message stream to Pixhawk
void PixhawkAP::updatePX4() {

	//uint64_t startTimestamp = usecSinceSystemBoot();
	//cout << "time elapsed: " << (int)(usecSinceSystemBoot() - startTimestamp) << " usec\n";

	if (heartbeat_time <= usecSinceSystemBoot()) {
		heartbeat_time += 1000000; // advance by 1 sec (1M usec) = 1 Hz
		sendHeartbeat();

		// send set_mode
		//cout << (int)hbBaseMode << " | " << (int)hbCustomMode << " :: " << (hbBaseMode != 189) << " | " << (hbCustomMode != 67371008) << endl;
		if (hbBaseMode == 189 && hbCustomMode != 67371008) {
			// set to 189 with custom mode to 67371008
			//cout << "Sent setmode of 189 and 67371008" << endl;
			mavlink_msg_set_mode_pack(sid, cid, &msg2, 1, 189, 67371008);
			sendMessage(&msg2);
		}
	}

	if (hil_sensor_time <= usecSinceSystemBoot()) {
		hil_sensor_time += 20000; // advance by 20 ms (20K usec) = 50 Hz
		sendHilSensor();
	}

	if (hil_gps_time <= usecSinceSystemBoot()) {
		hil_gps_time += 100000; // advance by 100 ms (100K usec) = 10 Hz
		sendHilGps();
	}
	
	// Sent twice for direct waypoint navigation (rather than 45 deg angle to line between previous and current waypoint)
	if (newWaypointSet < 2 && hbBaseMode == 189) {
		sendDynamicWaypoint(); // send rate determined by OCA
	}

	sendSetMode();

	// Update magnetometer values
	//if (mag_time <= usecSinceSystemBoot()) {
	//	mag_time += 5000000; // advance by 5 sec (5M usec) = 0.2 Hz
	//	updateMagValues();
	//}
}

void PixhawkAP::receive() {
	// receive serial data
	if (isSerialOpen()) { // prevents read attempts to closed ports
		if (getSerialDataWaiting() > 11000) { // serial buffer size is 12 KB
			cout << "\nWARNING: full serial buffer detected, indicating dropped MAVLink message(s)\n";
		}
		
		while (getSerialDataWaiting() > bufferSize) { // wait until we have enough data to fill buffer
			setSerialReadData(lpBuffer, bufferSize); // refill buffer
			while (bytesRead < bufferSize) { // read one byte at a time
				if (mavlink_parse_char(1, lpBuffer[bytesRead++], &rcvMsg, &mavStatus)) { // build msg
					talkingMavlink = true;
					recordMessage(rcvMsg.msgid, false, rcvMsg.len+8); // false = receiving
					
					if (0) {
						mavRcvd[rcvMsg.msgid]++;
						cout << "\r";
						for (int i = 0; i < 256; i++) {
							if (mavRcvd[i] > 0) {
								cout << i << "(" << mavRcvd[i] << ") ";
							}
						}
					}
					
					// Process mavlink messages
					switch (rcvMsg.msgid) {
					case MAVLINK_MSG_ID_HEARTBEAT:
						hbSid            = rcvMsg.sysid;
						hbCid            = rcvMsg.compid;
						hbType           = mavlink_msg_heartbeat_get_type(&rcvMsg);
						hbAutopilot      = mavlink_msg_heartbeat_get_autopilot(&rcvMsg);
						hbBaseMode       = mavlink_msg_heartbeat_get_base_mode(&rcvMsg);
						hbCustomMode     = mavlink_msg_heartbeat_get_custom_mode(&rcvMsg);
						hbSystemStatus   = mavlink_msg_heartbeat_get_system_status(&rcvMsg);
						hbMavlinkVersion = mavlink_msg_heartbeat_get_mavlink_version(&rcvMsg);

						//cout << "PX4 Heartbeat = type: " << (int)hbType <<
						//	" | autopilot: " << (int)hbAutopilot <<
						//	" | base_mode: " << (int)hbBaseMode <<
						//	" | custom_mode: " << (int)hbCustomMode <<
						//	" | system_status: " << (int)hbSystemStatus <<
						//	" | mavlink_version: " << (int)hbMavlinkVersion << endl;
						break;
					case MAVLINK_MSG_ID_MISSION_ITEM:
						setMiLat(mavlink_msg_mission_item_get_x(&rcvMsg));
						setMiLon(mavlink_msg_mission_item_get_y(&rcvMsg));
						setMiAlt(mavlink_msg_mission_item_get_z(&rcvMsg));
						break;
					case MAVLINK_MSG_ID_MISSION_REQUEST:
						if (mavlink_msg_mission_request_get_seq(&rcvMsg) == 0 &&
							mavlink_msg_mission_request_get_target_system(&rcvMsg) == 255 &&
							mavlink_msg_mission_request_get_target_component(&rcvMsg) == 0) {
							setMsnReqRcvd(true);
						}
						break;
					case MAVLINK_MSG_ID_MISSION_COUNT:
						setMcCnt(mavlink_msg_mission_count_get_count(&rcvMsg));
						break;
					case MAVLINK_MSG_ID_MISSION_ACK:
						if (mavlink_msg_mission_ack_get_type(&rcvMsg) == 0 &&
							mavlink_msg_mission_ack_get_target_system(&rcvMsg) == 255 &&
							mavlink_msg_mission_ack_get_target_component(&rcvMsg) == 0) {
							setMsnAckRcvd(true);
						}
						break;
					case MAVLINK_MSG_ID_HIL_CONTROLS:
						hcRollCtrl     =  mavlink_msg_hil_controls_get_roll_ailerons(&rcvMsg);
						hcPitchCtrl    = -mavlink_msg_hil_controls_get_pitch_elevator(&rcvMsg);
						hcYawCtrl      =  mavlink_msg_hil_controls_get_yaw_rudder(&rcvMsg);
						hcThrottleCtrl =  mavlink_msg_hil_controls_get_throttle(&rcvMsg);
						hcSysMode      =  mavlink_msg_hil_controls_get_mode(&rcvMsg);
						hcNavMode      =  mavlink_msg_hil_controls_get_nav_mode(&rcvMsg);
						//cout << "\rroll/pitch/yaw =\t" << hcRollCtrl << "\t" << hcPitchCtrl << "\t" << hcYawCtrl << "                                                   ";
						break;
					case MAVLINK_MSG_ID_STATUSTEXT:
						mavlink_msg_statustext_get_text(&rcvMsg, (char*)&text);
						if (strstr(text, "Waypoint too far") != nullptr) {
							waypointTooFar = true;
						} else if (strstr(text, "Not yet ready for mission, no position lock") != nullptr) {
							readyForMission = false;
						}
						//else if (strstr(text, "Waypoint 0 below home") != nullptr) {
						//	cout << "HELLO WORLD!" << endl;
						//	sendMessage(&msg2);
						//	mavlink_msg_command_long_pack(sid, cid, &msg2, 1, 1, 179, 0, 1, 0, 0, 0, 0, 0, 0);
						//}
						if (strcmp(getStatustexts(), "on") == 0) {
							cout << "\nStatus Text(" << getOwnship()->getID() << "): " << text << endl;
						}
						break;
					} // end switch
				} // end if
			} // end while true
			bytesRead = 0; // reset the lpBuffer index
		}
	} else {
		cout << "ERROR: serial port not connected, failure to connect to Pixhawk" << endl;
		_getch();
		exit(0);
	}
}

// SET_MODE (11)
void PixhawkAP::sendSetMode() {
	if (hbBaseMode == 189) {//&& hbCustomMode == 67371008) {
		return;
	}
	//else if (hbBaseMode == 65 || hbBaseMode == 97 || hbBaseMode == 225 || hbBaseMode == 189) {
	//	cout << "Base mode is: " << (int)hbBaseMode << " | ";
	//}

	if (hbBaseMode == 65) {
		// set to 97
		//cout << "Attempting to set to: 97" << endl;
		mavlink_msg_set_mode_pack(sid, cid, &msg2, 1, 97, 65536);
		sendMessage(&msg2);
	} else if (hbBaseMode == 97) {
		// set to 225
		//cout << "Attempting to set to: 225" << endl;
		mavlink_msg_set_mode_pack(sid, cid, &msg2, 1, 225, 65536);
		sendMessage(&msg2);
	} else if (hbBaseMode == 225) {
		hilModeSet = true;
		// set to 189
		//cout << "Attempting to set to: 189" << endl;
		mavlink_msg_set_mode_pack(sid, cid, &msg2, 1, 189, 262144); // 84148224
		sendMessage(&msg2);
	}
}

void PixhawkAP::initiateMavlink() {
	cout << "Initializing MAVLink stream over COM" << portNum << endl;
	sendMutex.lock();
	// "...sh /etc/init.d/rc.usb....."
	char* str = "\x0d\x0d\x0d\x73\x68\x20\x2f\x65\x74\x63\x2f\x69\x6e\x69\x74\x2e\x64\x2f\x72\x63\x2e\x75\x73\x62\x0a\x0d\x0d\x0d\x00\0";
	int len = strlen(str);
	int bytesSent = serial.SendData(str, len);
	sendMutex.unlock();
}

//------------------------------------------------------------------------------
// dynamics() is called by updateTC() and therefore a time-critical method.
// Autopilot updates are time-critical because control inputs must sync with
// the FDM (i.e. JSBSim) which is also a time-critical process.
//------------------------------------------------------------------------------

void PixhawkAP::dynamics(const LCreal dt) {
	// initialize HIL if not done so already
	if (!serial.IsOpened()) { // open serial connection if not already
		if (!connectToPixhawk()) { // attempt to open serial port to PX4
			cout << "ERROR: failed to establish connection to PX4." << endl;
			_getch();
			exit(0);
		}
	}

	// allow manual flight
	if (mode == 0) return;

	// get UAV
	Swarms::UAV* uav = dynamic_cast<Swarms::UAV*>(this->getOwnship());
	if (uav == 0) return;

	// get UAV's dynamics model (i.e. JSBSim)
	Eaagles::Dynamics::JSBSimModel* dm = dynamic_cast<Eaagles::Dynamics::JSBSimModel*>(uav->getDynamicsModel());
	if (dm == 0) return;

	// provide control inputs to JSBSim
	dm->setControlStickPitchInput(hcPitchCtrl);
	dm->setControlStickRollInput(hcRollCtrl);
	dm->setRudderPedalInput(hcYawCtrl);
	dm->setThrottles(&hcThrottleCtrl, 1);
	//cout << "\rpitch: " << hcPitchCtrl << " | roll: " << hcRollCtrl << " | yaw: " << hcYawCtrl << " | throttle: " << hcThrottleCtrl << "                             ";

	// collect data from Pixhawk
	receive();

	// pass data to Pixhawk
	if (talkingMavlink) {
		updatePX4(); // push UAV attitude/position/etc to PX4
	} else if (set_mode_time <= usecSinceSystemBoot()) {
		set_mode_time = usecSinceSystemBoot() + 3000000; // wait 3 sec before retrying initiation
		initiateMavlink();
	}

	BaseClass::updateData(dt);
}

//------------------------------------------------------------------------------
// USB connection management
//------------------------------------------------------------------------------

bool PixhawkAP::connectToPixhawk() {
	if (!serial.Open(portNum, 9600)) {
		cout << "Failed to open port (" << portNum << ") :(" << endl;
		return false;
	}
	cout << "Connected to PX4 over COM port " << portNum << "." << endl;
	return true;
}

bool PixhawkAP::sendBytes(char* bytes) {
	sendMutex.lock();
	int bytesSent = serial.SendData(bytes, strlen(bytes));
	sendMutex.unlock();
	return bytesSent > 0;
}

bool PixhawkAP::sendMessage(mavlink_message_t* msg) {
	recordMessage(msg->msgid, true, msg->len+8); // true = send
	sendMutex.lock();
	uint8_t buff[265];
	int buffLen = mavlink_msg_to_send_buffer(buff, msg);
	int bytesSent = serial.SendData((char*)buff, buffLen);
	sendMutex.unlock();

	return bytesSent > 0;
}

} // end Swarms namespace
} // end Eaagles namespace