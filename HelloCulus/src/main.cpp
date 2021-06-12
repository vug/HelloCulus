#include <iostream>
#include <iomanip>
#include <Windows.h>

#include <OVR_CAPI.h>
#include <Extras/OVR_Math.h>

void printHmdInfo(const ovrHmdDesc& desc) {
	std::cout << "Head Mounted Display Info" << std::endl;
	std::cout << "  Type: " << desc.Type << ". (14 is ovrHmd_CV1)" << std::endl;
	std::cout << "  Manufacturer: " << desc.Manufacturer << std::endl;
	std::cout << "  Firmware Version. Major: " << desc.FirmwareMajor << ", Minor: " << desc.FirmwareMinor << std::endl;
	std::cout << "  Resolution. Width: " << desc.Resolution.w << ", Height: " << desc.Resolution.h << std::endl;
	std::cout << "  Display Refresh Rate: " << desc.DisplayRefreshRate << std::endl;
	// AvailableHmdCaps, DefaultHmdCaps, AvailableTrackingCaps, DefaultTrackingCaps, DefaultEyeFov, MaxEyeFov
}

// Non-blocking keyboard input
DWORD numEvents;
INPUT_RECORD irInBuf[128]; // buffer to read into
HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE); // input buffer handle
bool keyboardInterrupt() {
	bool shouldExit = false;
	PeekConsoleInput(hStdin, irInBuf, 128, &numEvents);
	if (numEvents > 0) {
		ReadConsoleInput(hStdin, irInBuf, 128, &numEvents);
		for (int i = 0; i < numEvents; i++) {
			if (irInBuf[i].EventType != KEY_EVENT)
				continue;
			KEY_EVENT_RECORD ev = irInBuf[i].Event.KeyEvent;
			if (!ev.bKeyDown)
				continue;
			switch (ev.uChar.AsciiChar) {
			case 'q':
			case 'Q':
				shouldExit = true;
			}
			//std::cout << "key code: " << ev.wVirtualKeyCode << ", ...: " << ev.wVirtualScanCode << ", ...: " << ev.uChar.AsciiChar << std::endl;
		}
	}
	return shouldExit;
}

void printPositionAndOrientation(ovrTrackingState ts, int timeStep) {
	std::cout << "\rtimeStep: " << std::setw(5) << timeStep << std::flush;
	if (ts.StatusFlags & ovrStatus_OrientationTracked) {
		ovrPosef pose = ts.HeadPose.ThePose;
		ovrVector3f pos = pose.Position;
		OVR::Quatf orientation = pose.Orientation;
		float yaw, pitch, roll;
		orientation.GetYawPitchRoll(&yaw, &pitch, &roll);
		std::cout
			<< std::showpos << std::fixed << std::setprecision(3)
			<< " position: (" << pos.x << ", " << pos.y << ", " << pos.z << ")"
			<< " yaw/pitch/roll: (" << yaw << ", " << pitch << ", " << roll << ")"
			<< std::flush;
	}
}

int main() {
	std::cout << "Hello, Rift!" << std::endl;

	ovrResult result = ovr_Initialize(nullptr);
	if (OVR_FAILURE(result)) { std::cout << "Initialization failed with result code " << result << std::endl;  return result; }

	ovrSession session;
	ovrGraphicsLuid luid;
	result = ovr_Create(&session, &luid);
	if (OVR_FAILURE(result)) { std::cout << "Creation failed with result code " << result << std::endl; ovr_Shutdown(); return result; }

	ovrHmdDesc hmdDesc = ovr_GetHmdDesc(session);
	printHmdInfo(hmdDesc);

	std::cout << "Press Q to quit." << std::endl;
	int timeStep = 0;
	while (true) {
		if (keyboardInterrupt()) break;
		ovrTrackingState ts = ovr_GetTrackingState(session, ovr_GetTimeInSeconds(), ovrTrue);
		printPositionAndOrientation(ts, timeStep);
		timeStep++;
		Sleep(10);
	}
	std::cout << std::endl;

	ovr_Destroy(session);
	ovr_Shutdown();
	std::cout << "Bye, Rift!" << std::endl;
	return 0;
}