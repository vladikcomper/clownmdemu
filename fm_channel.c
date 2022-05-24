#include "fm_channel.h"

#include "clowncommon.h"

void FM_Channel_Constant_Initialise(FM_Channel_Constant *constant)
{
	FM_Operator_Constant_Initialise(&constant->operators);
}

void FM_Channel_State_Initialise(FM_Channel_State *state)
{
	unsigned int i;

	for (i = 0; i < CC_COUNT_OF(state->operators); ++i)
		FM_Operator_State_Initialise(&state->operators[i]);
}

void FM_Channel_SetFrequency(const FM_Channel *channel, unsigned int f_number_and_block)
{
	unsigned int i;

	for (i = 0; i < CC_COUNT_OF(channel->state->operators); ++i)
	{
		const FM_Operator fm_operator = {&channel->constant->operators, &channel->state->operators[i]};

		FM_Operator_SetFrequency(&fm_operator, f_number_and_block);
	}
}

void FM_Channel_SetFeedbackAndAlgorithm(const FM_Channel *channel, unsigned int feedback, unsigned int algorithm)
{
	channel->state->feedback = feedback;
	channel->state->algorithm = algorithm;
}

void FM_Channel_SetDetuneAndMultiplier(const FM_Channel *channel, unsigned int operator_index, unsigned int detune, unsigned int multiplier)
{
	const FM_Operator fm_operator = {&channel->constant->operators, &channel->state->operators[operator_index]};

	FM_Operator_SetDetuneAndMultiplier(&fm_operator, detune, multiplier);
}

void FM_Channel_SetTotalLevel(const FM_Channel *channel, unsigned int operator_index, unsigned int total_level)
{
	const FM_Operator fm_operator ={&channel->constant->operators, &channel->state->operators[operator_index]};

	FM_Operator_SetTotalLevel(&fm_operator, total_level);
}

int FM_Channel_GetSample(const FM_Channel *channel)
{
	const FM_Operator operator1 = {&channel->constant->operators, &channel->state->operators[0]};
	const FM_Operator operator2 = {&channel->constant->operators, &channel->state->operators[2]}; /* Yes, these really are swapped. */
	const FM_Operator operator3 = {&channel->constant->operators, &channel->state->operators[1]};
	const FM_Operator operator4 = {&channel->constant->operators, &channel->state->operators[3]};

	int operator_1_sample;
	int operator_2_sample;
	int operator_3_sample;
	int operator_4_sample;
	int sample;

	/* Feed the operators into each other to produce the final sample. */
	/* Note that the operators output a 14-bit sample, meaning that, if all four are summed, then the result is a 16-bit sample,
	   so there is no possibility of overflow. */
	switch (channel->state->algorithm)
	{
		/* TODO: Feedback. */
		case 0:
			/* "Four serial connection mode". */
			operator_1_sample = FM_Operator_Process(&operator1, 0);
			operator_2_sample = FM_Operator_Process(&operator2, operator_1_sample);
			operator_3_sample = FM_Operator_Process(&operator3, operator_2_sample);
			operator_4_sample = FM_Operator_Process(&operator4, operator_3_sample);

			sample = operator_4_sample;

			break;

		case 1:
			/* "Three double modulation serial connection mode". */
			operator_1_sample = FM_Operator_Process(&operator1, 0);
			operator_2_sample = FM_Operator_Process(&operator2, 0);

			operator_3_sample = FM_Operator_Process(&operator3, operator_1_sample + operator_2_sample);
			operator_4_sample = FM_Operator_Process(&operator4, operator_3_sample);

			sample = operator_4_sample;

			break;

		case 2:
			/* "Double modulation mode (1)". */
			operator_1_sample = FM_Operator_Process(&operator1, 0);

			operator_2_sample = FM_Operator_Process(&operator2, 0);
			operator_3_sample = FM_Operator_Process(&operator3, operator_2_sample);

			operator_4_sample = FM_Operator_Process(&operator4, operator_1_sample + operator_3_sample);

			sample = operator_4_sample;

			break;

		case 3:
			/* "Double modulation mode (2)". */
			operator_1_sample = FM_Operator_Process(&operator1, 0);
			operator_2_sample = FM_Operator_Process(&operator2, operator_1_sample);

			operator_3_sample = FM_Operator_Process(&operator3, 0);

			operator_4_sample = FM_Operator_Process(&operator4, operator_2_sample + operator_3_sample);

			sample = operator_4_sample;

			break;

		case 4:
			/* "Two serial connection and two parallel modes". */
			operator_1_sample = FM_Operator_Process(&operator1, 0);
			operator_2_sample = FM_Operator_Process(&operator2, operator_1_sample);

			operator_3_sample = FM_Operator_Process(&operator3, 0);
			operator_4_sample = FM_Operator_Process(&operator4, operator_3_sample);

			sample = operator_2_sample + operator_4_sample;

			break;

		case 5:
			/* "Common modulation 3 parallel mode". */
			operator_1_sample = FM_Operator_Process(&operator1, 0);

			operator_2_sample = FM_Operator_Process(&operator2, operator_1_sample);
			operator_3_sample = FM_Operator_Process(&operator3, operator_1_sample);
			operator_4_sample = FM_Operator_Process(&operator4, operator_1_sample);

			sample = operator_2_sample + operator_3_sample + operator_4_sample;

			break;

		case 6:
			/* "Two serial connection + two sine mode". */
			operator_1_sample = FM_Operator_Process(&operator1, 0);
			operator_2_sample = FM_Operator_Process(&operator2, operator_1_sample);

			operator_3_sample = FM_Operator_Process(&operator3, 0);

			operator_4_sample = FM_Operator_Process(&operator4, 0);

			sample = operator_2_sample + operator_3_sample + operator_4_sample;

			break;

		case 7:
			/* "Four parallel sine synthesis mode". */
			operator_1_sample = FM_Operator_Process(&operator1, 0);

			operator_2_sample = FM_Operator_Process(&operator2, 0);

			operator_3_sample = FM_Operator_Process(&operator3, 0);

			operator_4_sample = FM_Operator_Process(&operator4, 0);

			sample = operator_1_sample + operator_2_sample + operator_3_sample + operator_4_sample;

			break;
	}

	return sample;
}
