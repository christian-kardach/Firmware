#include "CommandProcessor.h"
#include "NexDome.h"
#include "../TA.NexDome.Shutter/CommandProcessor.h"
#include "Version.h"

CommandProcessor::CommandProcessor(MicrosteppingMotor& rotator, PersistentSettings& settings, XBeeStateMachine& machine,
                                   HomeSensor& homeSensor)
	: rotator(rotator), settings(settings), machine(machine), homeSensor(homeSensor) { }

Response CommandProcessor::ForwardToShutter(Command& command)
	{
	machine.SendToRemoteXbee(command.RawCommand);
	//ToDo: should the response always be successful?
	return Response::FromSuccessfulCommand(command);
	}

Response CommandProcessor::HandleCommand(Command& command)
	{
	if (command.IsShutterCommand())
		{
		ForwardToShutter(command);
		return Response::NoResponse(command);
		}

	if (command.IsRotatorCommand())
		{
		if (command.Verb == "AR") return HandleAR(command); // Read firmware version
		if (command.Verb == "AW") return HandleAW(command); // Read firmware version
		if (command.Verb == "FR") return HandleFR(command); // Read firmware version
		if (command.Verb == "GA") return HandleGA(command); // Goto Azimuth (rotator only)
		if (command.Verb == "GH") return HandleGH(command); // Goto Home Sensor (rotator only)
		if (command.Verb == "SW") return HandleSW(command); // Stop motor
		if (command.Verb == "PR") return HandlePR(command); // Position read
		if (command.Verb == "PW") return HandlePW(command); // Position write (sync)
		if (command.Verb == "RR") return HandleRR(command); // Range Read (get limit of travel)
		if (command.Verb == "RW") return HandleRW(command); // Range Write (set limit of travel)
		if (command.Verb == "VR") return HandleVR(command); // Read maximum motor speed
		if (command.Verb == "VW") return HandleVW(command); // Read maximum motor speed
		if (command.Verb == "ZD") return HandleZD(command); // Reset to factory settings (load defaults).
		if (command.Verb == "ZR") return HandleZR(command); // Load settings from persistent storage
		if (command.Verb == "ZW") return HandleZW(command); // Write settings to persistent storage
		}
	if (command.IsSystemCommand())
		{
		// There are currently no system commands
		}
	return Response::Error();
	}


Response CommandProcessor::HandleAR(Command& command) const
	{
	const auto rampTime = settings.motor.rampTimeMilliseconds;
	return Response::FromInteger(command, rampTime);
	}


Response CommandProcessor::HandleGA(Command& command)
	{
	//ToDo: This is temporary code for testing and needs to be re-done
	const auto microstepsPerDegree = ROTATOR_FULL_REVOLUTION_MICROSTEPS / 360.0;
	const auto target = targetStepPosition(command.StepPosition * microstepsPerDegree);
	std::cout << "Target " << target << std::endl;
	rotator.MoveToPosition(target);
	return Response::FromSuccessfulCommand(command);
	}

Response CommandProcessor::HandleGH(Command& command)
	{
	auto delta = deltaSteps(settings.home.position);
	if (delta != 0)
		HomeSensor::findHome(sgn(delta));
	return Response::FromSuccessfulCommand(command);
	}

//Response CommandProcessor::HandleMI(Command& command)
//	{
//	// Commands are in whole steps, motors operate in microsteps, so we must convert.
//	auto motor = GetMotor(command);
//	auto microStepsToMove = StepsToMicrosteps(command.StepPosition);
//	auto targetPosition = motor->CurrentPosition() - microStepsToMove;
//	if (targetPosition < 0)
//		return Response::Error();
//	motor->MoveToPosition(targetPosition);
//	return Response::FromSuccessfulCommand(command);
//	}
//
//Response CommandProcessor::HandleMO(Command& command)
//	{
//	// Commands are in whole steps, motors operate in microsteps, so we must convert.
//	auto motor = GetMotor(command);
//	auto microStepsToMove = StepsToMicrosteps(command.StepPosition);
//	auto targetPosition = motor->CurrentPosition() + microStepsToMove;
//	if (targetPosition > motor->LimitOfTravel())
//		return Response::Error();
//	motor->MoveToPosition(targetPosition);
//	return Response::FromSuccessfulCommand(command);
//	}

Response CommandProcessor::HandleAW(Command& command)
	{
	auto rampTime = command.StepPosition;
	// The minimum ramp time is 100ms, fail if the user tries to set it lower.
	if (rampTime < MIN_RAMP_TIME)
		return Response::Error();
	rotator.SetRampTime(rampTime);
	return Response::FromSuccessfulCommand(command);
	}

Response CommandProcessor::HandleSW(Command& command)
	{
	rotator.HardStop();
	return Response::FromSuccessfulCommand(command);
	}

Response CommandProcessor::HandleZW(Command& command)
	{
	settings.Save();
	return Response::FromSuccessfulCommand(command);
	}

Response CommandProcessor::HandleZR(Command& command)
	{
	settings = PersistentSettings::Load();
	return Response::FromSuccessfulCommand(command);
	}

Response CommandProcessor::HandleZD(Command& command)
	{
	settings = PersistentSettings();
	settings.Save();
	return Response::FromSuccessfulCommand(command);
	}

Response CommandProcessor::HandlePR(Command& command)
	{
	auto position = microstepsToSteps(rotator.CurrentPosition());
	auto response = Response::FromPosition(command, position);
	return response;
	}

Response CommandProcessor::HandlePW(Command& command)
	{
	auto microsteps = stepsToMicrosteps(command.StepPosition);
	rotator.SetCurrentPosition(microsteps);
	return Response::FromSuccessfulCommand(command);
	}

Response CommandProcessor::HandleRW(Command& command)
	{
	auto microsteps = stepsToMicrosteps(command.StepPosition);
	rotator.SetLimitOfTravel(microsteps);
	return Response::FromSuccessfulCommand(command);
	}

Response CommandProcessor::HandleRR(Command& command)
	{
	auto range = microstepsToSteps(rotator.LimitOfTravel());
	return Response::FromPosition(command, range);
	}

Response CommandProcessor::HandleFR(Command& command)
	{
	std::string message;
	message.append("FR");
	message.append(SemanticVersion);
	return Response{message};
	}

Response CommandProcessor::HandleVR(Command& command)
	{
	auto maxSpeed = rotator.MaximumSpeed();
	return Response::FromPosition(command, microstepsToSteps(maxSpeed));
	}

Response CommandProcessor::HandleVW(Command& command)
	{
	uint16_t speed = stepsToMicrosteps(command.StepPosition);
	if (speed < rotator.MinimumSpeed())
		return Response::Error();
	rotator.SetMaximumSpeed(speed);
	return Response::FromSuccessfulCommand(command);
	}


Response CommandProcessor::HandleX(Command& command)
	{
	if (rotator.IsMoving())
		return Response::FromInteger(command, 2);
	return Response::FromInteger(command, 0);
	}

/*
 * Computes the final target step position taking into account the shortest movement direction.
 */
int32_t CommandProcessor::targetStepPosition(const uint32_t toMicrostepPosition) const
	{
	return getNormalizedPositionInMicrosteps() + deltaSteps(toMicrostepPosition);
	}

/*
 * Computes the change in step position to reach a target taking into account the shortest movement direction.
 */
int32_t CommandProcessor::deltaSteps(const uint32_t toMicrostepPosition) const
	{
	const int32_t halfway = settings.microstepsPerRotation / 2;
	const uint32_t fromMicrostepPosition = getNormalizedPositionInMicrosteps();
	int32_t delta = toMicrostepPosition - fromMicrostepPosition;
	if (delta == 0)
		return 0;
	if (delta > halfway)
		delta -= settings.microstepsPerRotation;
	if (delta < -halfway)
		delta += settings.microstepsPerRotation;
	return delta;
	}

inline int32_t CommandProcessor::microstepsToSteps(int32_t microsteps)
	{
	return microsteps / MICROSTEPS_PER_STEP;
	}

inline int32_t CommandProcessor::stepsToMicrosteps(int32_t wholeSteps)
	{
	return wholeSteps * MICROSTEPS_PER_STEP;
	}

uint32_t CommandProcessor::getNormalizedPositionInMicrosteps() const
	{
	auto position = rotator.CurrentPosition();
	while (position < 0)
		position += ROTATOR_FULL_REVOLUTION_MICROSTEPS;
	return position;
	}

inline int32_t CommandProcessor::getPositionInWholeSteps() const
	{
	return microstepsToSteps(getNormalizedPositionInMicrosteps());
	}

float CommandProcessor::getAzimuth() const
	{
	const auto degreesPerStep = 360.0 / settings.microstepsPerRotation;
	return getPositionInWholeSteps() * degreesPerStep;
	}
