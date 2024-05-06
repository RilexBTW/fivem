#define BLOCK_NATIVE(x)			\
	case (x):					\
	{							\
		handler = noOpHandler;	\
		break;					\
	}							\

auto noOpHandler = [](rage::scrNativeCallContext* ctx)
{

};

switch (hash)
{
	BLOCK_NATIVE(0x39551B76); // SET_BIT
	BLOCK_NATIVE(0x66D57CC4); // CLEAR_BIT
	BLOCK_NATIVE(0x14DD5F87); // SET_BITS_IN_RANGE
	BLOCK_NATIVE(0x58AE7C1D); // GET_BITS_IN_RANGE
}

#undef BLOCK_NATIVE
