// XBeeStartupState.h

#ifndef _XBEERESETCOMPLETESTATE_h
#define _XBEERESETCOMPLETESTATE_h

#if defined(ARDUINO) && ARDUINO >= 100
#include "arduino.h"
#else
#include "WProgram.h"
#endif
#include <XBeeStateMachine.h>

class XBeeResetCompleteState : public IXBeeState
{
public:
	const std::string name() override { return "Factory Reset Complete"; }
	void OnEnter() override;
	void OnTimerExpired() override;
	explicit XBeeResetCompleteState(XBeeStateMachine& machine) : IXBeeState(machine) {}
};

#endif

