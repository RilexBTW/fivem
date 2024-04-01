#pragma once
#include <StdInc.h>

class CommandLineOption;

static CommandLineOption** g_curOption;

class CommandLineOption
{
public:
	const char* name;
	const char* description;
	union
	{
		uint64_t value;
	};
	CommandLineOption* next;

public:
	CommandLineOption(const char* name, const char* description)
		: name(name), description(description), value(0), next(nullptr)
	{
		if (IsRunningTests())
		{
			return;
		}

		next = *g_curOption;
		*g_curOption = this;
	}
};
