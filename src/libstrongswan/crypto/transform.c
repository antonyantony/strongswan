/*
 * Copyright (C) 2006-2009 Martin Willi
 *
 * Copyright (C) secunet Security Networks AG
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.  See <http://www.fsf.org/copyleft/gpl.txt>.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 */

#include <crypto/transform.h>
#include <crypto/hashers/hasher.h>
#include <crypto/rngs/rng.h>
#include <crypto/kdfs/kdf.h>

ENUM_BEGIN(transform_type_names, ENCRYPTION_ALGORITHM, ADDITIONAL_KEY_EXCHANGE_7,
	"ENCRYPTION_ALGORITHM",
	"PSEUDO_RANDOM_FUNCTION",
	"INTEGRITY_ALGORITHM",
	"KEY_EXCHANGE_METHOD",
	"EXTENDED_SEQUENCE_NUMBERS",
	"ADDITIONAL_KEY_EXCHANGE_1",
	"ADDITIONAL_KEY_EXCHANGE_2",
	"ADDITIONAL_KEY_EXCHANGE_3",
	"ADDITIONAL_KEY_EXCHANGE_4",
	"ADDITIONAL_KEY_EXCHANGE_5",
	"ADDITIONAL_KEY_EXCHANGE_6",
	"ADDITIONAL_KEY_EXCHANGE_7");
ENUM_NEXT(transform_type_names, SUB_SA_KDF, SUB_SA_KDF, ADDITIONAL_KEY_EXCHANGE_7,
	"SUB_SA_KDF");
ENUM_NEXT(transform_type_names, HASH_ALGORITHM, KEY_DERIVATION_FUNCTION, SUB_SA_KDF,
	"HASH_ALGORITHM",
	"RANDOM_NUMBER_GENERATOR",
	"AEAD_ALGORITHM",
	"COMPRESSION_ALGORITHM",
	"EXTENDED OUTPUT FUNCTION",
	"DETERMINISTIC RANDOM BIT GENERATOR",
	"KEY_DERIVATION_FUNCTION");
ENUM_END(transform_type_names, KEY_DERIVATION_FUNCTION);

ENUM_BEGIN(extended_sequence_numbers_names, NO_EXT_SEQ_NUMBERS, EXT_SEQ_NUMBERS,
	"NO_EXT_SEQ",
	"EXT_SEQ");
ENUM_NEXT(extended_sequence_numbers_names, EESP_SEQ_64BIT, EESP_SEQ_NONE, EXT_SEQ_NUMBERS,
	"EESP_SEQ_64BIT",
	"EESP_SEQ_NONE");
ENUM_END(extended_sequence_numbers_names, EESP_SEQ_NONE);

ENUM(sub_sa_kdf_names, SSKDF_NONE, SSKDF_AES256_CMAC,
	"SSKDF_NONE",
	"SSKDF_HKDF_SHA2_256",
	"SSKDF_HKDF_SHA2_384",
	"SSKDF_HKDF_SHA2_512",
	"SSKDF_AES256_CMAC",
);

/**
 * See header
 */
enum_name_t* transform_get_enum_names(transform_type_t type)
{
	switch (type)
	{
		case HASH_ALGORITHM:
			return hash_algorithm_names;
		case RANDOM_NUMBER_GENERATOR:
			return rng_quality_names;
		case AEAD_ALGORITHM:
		case ENCRYPTION_ALGORITHM:
			return encryption_algorithm_names;
		case PSEUDO_RANDOM_FUNCTION:
			return pseudo_random_function_names;
		case INTEGRITY_ALGORITHM:
			return integrity_algorithm_names;
		case KEY_EXCHANGE_METHOD:
		case ADDITIONAL_KEY_EXCHANGE_1:
		case ADDITIONAL_KEY_EXCHANGE_2:
		case ADDITIONAL_KEY_EXCHANGE_3:
		case ADDITIONAL_KEY_EXCHANGE_4:
		case ADDITIONAL_KEY_EXCHANGE_5:
		case ADDITIONAL_KEY_EXCHANGE_6:
		case ADDITIONAL_KEY_EXCHANGE_7:
			return key_exchange_method_names;
		case EXTENDED_SEQUENCE_NUMBERS:
			return extended_sequence_numbers_names;
		case SUB_SA_KDF:
			return sub_sa_kdf_names;
		case EXTENDED_OUTPUT_FUNCTION:
			return ext_out_function_names;
		case DETERMINISTIC_RANDOM_BIT_GENERATOR:
			return drbg_type_names;
		case KEY_DERIVATION_FUNCTION:
			return key_derivation_function_names;
		case COMPRESSION_ALGORITHM:
			break;
	}
	return NULL;
}
