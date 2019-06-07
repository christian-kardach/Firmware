#ifndef _RESPONSE_h
#define _RESPONSE_h

#if defined(ARDUINO) && ARDUINO >= 100
#include "Arduino.h"
#else
#include "WProgram.h"
#endif

#include "Command.h"

struct Response
{
	static const std::string terminator;
	static const std::string header;
	std::string Message;

	friend std::ostream& operator<<(std::ostream&, const Response&);

	/*
		Creates an error response.
	*/
	static Response Error();

	/*
		Creates a success response by echoing the command verb,
		terminated with a '#'.
	*/
	static Response FromSuccessfulCommand(Command& command);

	/*
		Creates a response consisting of the command verb,
		plus an unsigned integer (usually a motor step position).
	*/
	static Response FromPosition(Command& command, uint32_t position);

	/*
		Creates a response consisting of just an integer and the terminator.
	*/
	static Response FromInteger(Command& command, int i);
	static Response NoResponse(Command& command);
};

#endif