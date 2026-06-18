/*
 * Copyright (C) 2012-2019 Tobias Brunner
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

/**
 * @defgroup transforms transforms
 * @{ @ingroup crypto
 */

#ifndef TRANSFORM_H_
#define TRANSFORM_H_

typedef enum transform_type_t transform_type_t;

#include <utils/utils.h>

/**
 * EESPv0 provisional IKEv2 Transform Type values (draft-ietf-ipsecme-eesp-ikev2).
 * Update when IANA assigns: https://www.iana.org/assignments/ikev2-parameters
 */
/** Transform Type for SSKDF (TBD2, unassigned range 15-240) */
#define SUB_SA_KDF_TYPE_ID    241
/** Sequence Numbers transform IDs for EESPv0 (TBD5/TBD6, private-use 1024-65535) */
#define EESP_SEQ_64BIT_ID    1024
#define EESP_SEQ_NONE_ID     1025

/**
 * Type of a transform, as in IKEv2 RFC 3.3.2.
 */
enum transform_type_t {
	ENCRYPTION_ALGORITHM = 1,
	PSEUDO_RANDOM_FUNCTION = 2,
	INTEGRITY_ALGORITHM = 3,
	KEY_EXCHANGE_METHOD = 4,
	EXTENDED_SEQUENCE_NUMBERS = 5,
	ADDITIONAL_KEY_EXCHANGE_1 = 6,
	ADDITIONAL_KEY_EXCHANGE_2 = 7,
	ADDITIONAL_KEY_EXCHANGE_3 = 8,
	ADDITIONAL_KEY_EXCHANGE_4 = 9,
	ADDITIONAL_KEY_EXCHANGE_5 = 10,
	ADDITIONAL_KEY_EXCHANGE_6 = 11,
	ADDITIONAL_KEY_EXCHANGE_7 = 12,
	SUB_SA_KDF = SUB_SA_KDF_TYPE_ID,
	HASH_ALGORITHM = 256,
	RANDOM_NUMBER_GENERATOR = 257,
	AEAD_ALGORITHM = 258,
	COMPRESSION_ALGORITHM = 259,
	EXTENDED_OUTPUT_FUNCTION = 260,
	DETERMINISTIC_RANDOM_BIT_GENERATOR = 261,
	KEY_DERIVATION_FUNCTION = 262,
};

/**
 * Maximum number of additional key exchanges.
 */
#define MAX_ADDITIONAL_KEY_EXCHANGES (ADDITIONAL_KEY_EXCHANGE_7 - \
									  ADDITIONAL_KEY_EXCHANGE_1 + 1)

/**
 * enum names for transform_type_t.
 */
extern enum_name_t *transform_type_names;

/**
 * Get the enum names for a specific transform type.
 *
 * @param type		type of transform to get enum names for
 * @return			enum names
 */
enum_name_t *transform_get_enum_names(transform_type_t type);

/**
 * Check if the given transform type is used to negotiate a key exchange.
 *
 * @param type		type of transform to check
 * @return			TRUE if the transform type negotiates a key exchange
 */
static inline bool is_ke_transform(transform_type_t type)
{
	return type == KEY_EXCHANGE_METHOD || (ADDITIONAL_KEY_EXCHANGE_1 <= type &&
										   type <= ADDITIONAL_KEY_EXCHANGE_7);
}

/**
 * Sequence Numbers transform IDs (IKEv2 Transform Type 5, RFC 9827).
 * Values 0/1 are used for ESP/AH; 1024/1025 are for EESPv0 only.
 */
enum extended_sequence_numbers_t {
	/* ESP/AH: 32-bit sequential numbers (formerly "no ESN") */
	NO_EXT_SEQ_NUMBERS = 0,
	/* ESP/AH: partially transmitted 64-bit sequential numbers (formerly "ESN") */
	EXT_SEQ_NUMBERS = 1,
	/* EESPv0: 64-bit sequential numbers with replay protection */
	EESP_SEQ_64BIT = EESP_SEQ_64BIT_ID,
	/* EESPv0: no sequence number field (replay protection disabled) */
	EESP_SEQ_NONE = EESP_SEQ_NONE_ID,
};

/**
 * enum strings for extended_sequence_numbers_t.
 */
extern enum_name_t *extended_sequence_numbers_names;

/**
 * Sub SA Key Derivation Function algorithms for EESPv0
 * (IKEv2 Transform Type TBD2, draft-ietf-ipsecme-eesp-ikev2).
 */
typedef enum sub_sa_kdf_t sub_sa_kdf_t;
enum sub_sa_kdf_t {
	SSKDF_NONE            = 0,
	SSKDF_HKDF_SHA2_256   = 1,
	SSKDF_HKDF_SHA2_384   = 2,
	SSKDF_HKDF_SHA2_512   = 3,
	SSKDF_AES256_CMAC     = 4,
};

/**
 * enum strings for sub_sa_kdf_t.
 */
extern enum_name_t *sub_sa_kdf_names;

#endif /** TRANSFORM_H_ @}*/
