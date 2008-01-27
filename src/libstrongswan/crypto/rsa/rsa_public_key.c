/**
 * @file rsa_public_key.c
 * 
 * @brief Implementation of rsa_public_key_t.
 * 
 */

/*
 * Copyright (C) 2005 Jan Hutter
 * Copyright (C) 2005-2006 Martin Willi
 * Copyright (C) 2007-2008 Andreas Steffen
 *
 * Hochschule fuer Technik Rapperswil
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
 *
 * RCSID $Id$
 */
 
#include <gmp.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#include "rsa_public_key.h"

#include <debug.h>
#include <utils/randomizer.h>
#include <crypto/hashers/hasher.h>
#include <asn1/asn1.h>
#include <asn1/pem.h>

/* ASN.1 definition of RSApublicKey */
static const asn1Object_t pubkeyObjects[] = {
	{ 0, "RSAPublicKey",		ASN1_SEQUENCE,     ASN1_OBJ  }, /*  0 */
	{ 1,   "modulus",			ASN1_INTEGER,      ASN1_BODY }, /*  1 */
	{ 1,   "publicExponent",	ASN1_INTEGER,      ASN1_BODY }, /*  2 */
};

#define PUB_KEY_RSA_PUBLIC_KEY		0
#define PUB_KEY_MODULUS				1
#define PUB_KEY_EXPONENT			2
#define PUB_KEY_ROOF				3

/* ASN.1 definition of digestInfo */
static const asn1Object_t digestInfoObjects[] = {
	{ 0, "digestInfo",			ASN1_SEQUENCE,		ASN1_OBJ  }, /*  0 */
	{ 1,   "digestAlgorithm",	ASN1_EOC,			ASN1_RAW  }, /*  1 */
	{ 1,   "digest",			ASN1_OCTET_STRING,	ASN1_BODY }, /*  2 */
};

#define DIGEST_INFO					0
#define DIGEST_INFO_ALGORITHM		1
#define DIGEST_INFO_DIGEST			2
#define DIGEST_INFO_ROOF			3

typedef struct private_rsa_public_key_t private_rsa_public_key_t;

/**
 * Private data structure with signing context.
 */
struct private_rsa_public_key_t {
	/**
	 * Public interface for this signer.
	 */
	rsa_public_key_t public;
	
	/**
	 * Public modulus.
	 */
	mpz_t n;
	
	/**
	 * Public exponent.
	 */
	mpz_t e;
	
	/**
	 * Keysize in bytes.
	 */
	size_t k;
	
	/**
	 * Keyid formed as a SHA-1 hash of a publicKeyInfo object
	 */
	chunk_t keyid;

	/**
	 * @brief Implements the RSAEP algorithm specified in PKCS#1.
	 * 
	 * @param this		calling object
	 * @param data		data to process
	 * @return			processed data
	 */
	chunk_t (*rsaep) (const private_rsa_public_key_t *this, chunk_t data);
		
	/**
	 * @brief Implements the RSASVP1 algorithm specified in PKCS#1.
	 * 
	 * @param this		calling object
	 * @param data		data to process
	 * @return			processed data
	 */
	chunk_t (*rsavp1) (const private_rsa_public_key_t *this, chunk_t data);
};

/**
 * Implementation of private_rsa_public_key_t.rsaep and private_rsa_public_key_t.rsavp1
 */
static chunk_t rsaep(const private_rsa_public_key_t *this, chunk_t data)
{
	mpz_t m, c;
	chunk_t encrypted;
	
	mpz_init(c);
	mpz_init(m);
	
	mpz_import(m, data.len, 1, 1, 1, 0, data.ptr);
	
	mpz_powm(c, m, this->e, this->n);

    encrypted.len = this->k;
    encrypted.ptr = mpz_export(NULL, NULL, 1, encrypted.len, 1, 0, c);
	
	mpz_clear(c);
	mpz_clear(m);	
	
	return encrypted;
}

/**
 * Implementation of rsa_public_key_t.eme_pkcs1_encrypt.
 */
static status_t pkcs1_encrypt(private_rsa_public_key_t *this,
							  chunk_t in, chunk_t *out)
{
	chunk_t em;
	u_char *pos;
	int padding = this->k - in.len - 3;

	if (padding < 8)
	{
		DBG1("rsa padding of %d bytes is too small", padding);
		return FAILED;
	}
	em.len = this->k;
	em.ptr = pos = malloc(em.len);
	
	/* add padding according to PKCS#1 7.2.1 1.+2. */
	*pos++ = 0x00;
    *pos++ = 0x02;

    /* pad with pseudo random bytes unequal to zero */
	{
		randomizer_t *randomizer = randomizer_create();

	    /* pad with pseudo random bytes unequal to zero */
		while (padding--)
		{
			randomizer->get_pseudo_random_bytes(randomizer, 1, pos);
			while (!*pos)
			{
				randomizer->get_pseudo_random_bytes(randomizer, 1, pos);
			}
			pos++;
		}
		randomizer->destroy(randomizer);
	}

	/* append the padding terminator */
	*pos++ = 0x00;

	/* now add the data */
	memcpy(pos, in.ptr, in.len);
	*out = this->rsaep(this, em);
	free(em.ptr);
	return SUCCESS;
}

/**
 * Implementation of rsa_public_key.verify_emsa_pkcs1_signature.
 */
static status_t verify_emsa_pkcs1_signature(const private_rsa_public_key_t *this,
											hash_algorithm_t algorithm,
											chunk_t data, chunk_t signature)
{
	chunk_t em_ori, em;
	status_t res = FAILED;
	
	/* remove any preceding 0-bytes from signature */
	while (signature.len && *(signature.ptr) == 0x00)
	{
		signature.len -= 1;
		signature.ptr++;
	}
	
	if (signature.len > this->k)
	{
		return INVALID_ARG;
	}
	
	/* unpack signature */
	em_ori = em = this->rsavp1(this, signature);
	
	/* result should look like this:
	 * EM = 0x00 || 0x01 || PS || 0x00 || T. 
	 * PS = 0xFF padding, with length to fill em
	 * T = oid || hash
	 */
	
	/* check magic bytes */
	if (*(em.ptr) != 0x00 || *(em.ptr+1) != 0x01)
	{
		DBG2("incorrect padding - probably wrong RSA key");
		goto end;
	}
	em.ptr += 2;
	em.len -= 2;
	
	/* find magic 0x00 */
	while (em.len > 0)
	{
		if (*em.ptr == 0x00)
		{
			/* found magic byte, stop */
			em.ptr++;
			em.len--;
			break;
		}
		else if (*em.ptr != 0xFF)
		{
			/* bad padding, decryption failed ?!*/
			goto end;
		}
		em.ptr++;
		em.len--;
	}

	if (em.len == 0)
	{
		/* no digestInfo found */
		goto end;
	}

	/* parse ASN.1-based digestInfo */
	{
		asn1_ctx_t ctx;
		chunk_t object;
		u_int level;
		int objectID = 0;
		hash_algorithm_t hash_algorithm = HASH_UNKNOWN;

		asn1_init(&ctx, em, 0, FALSE, FALSE);

		while (objectID < DIGEST_INFO_ROOF)
		{
			if (!extract_object(digestInfoObjects, &objectID, &object, &level, &ctx))
			{
				goto end;
			}
			switch (objectID)
			{
				case DIGEST_INFO:
					if (em.len > object.len)
					{
						DBG1("digestInfo field in signature is followed by %u surplus bytes",
							 em.len - object.len);
						goto end;
					}
					break;
				case DIGEST_INFO_ALGORITHM:
					{
						int hash_oid = parse_algorithmIdentifier(object, level+1, NULL);

						hash_algorithm = hasher_algorithm_from_oid(hash_oid);
						if (hash_algorithm == HASH_UNKNOWN
						|| (algorithm != HASH_UNKNOWN && hash_algorithm != algorithm))
						{
							DBG1("wrong hash algorithm used in signature");
							goto end;
						}
					}
					break;
				case DIGEST_INFO_DIGEST:
					{
						chunk_t hash;
						hasher_t *hasher = hasher_create(hash_algorithm);

						if (object.len != hasher->get_hash_size(hasher))
						{
							DBG1("hash size in signature is %u bytes instead of %u bytes",
								 object.len, hasher->get_hash_size(hasher));
							hasher->destroy(hasher);
							goto end;
						}

						/* build our own hash */
						hasher->allocate_hash(hasher, data, &hash);
						hasher->destroy(hasher);
	
						/* compare the hashes */
						res = memeq(object.ptr, hash.ptr, hash.len) ? SUCCESS : FAILED;
						free(hash.ptr);
					}
					break;
				default:
					break;
			}
			objectID++;
		}
	}

end:
	free(em_ori.ptr);
	return res;
}


/**
 * Implementation of rsa_public_key_t.get_modulus.
 */
static mpz_t *get_modulus(const private_rsa_public_key_t *this)
{
	return (mpz_t*)&this->n;
}

/**
 * Implementation of rsa_public_key_t.get_keysize.
 */
static size_t get_keysize(const private_rsa_public_key_t *this)
{
	return this->k;
}

/**
 * Build a DER-encoded publicKeyInfo object from an RSA public key.
 * Also used in rsa_private_key.c.
 */
chunk_t rsa_public_key_info_to_asn1(const mpz_t n, const mpz_t e)
{
	chunk_t publicKey = asn1_wrap(ASN1_SEQUENCE, "mm",
								 asn1_integer_from_mpz(n),
								 asn1_integer_from_mpz(e));

	return asn1_wrap(ASN1_SEQUENCE, "cm",
				asn1_algorithmIdentifier(OID_RSA_ENCRYPTION),
				asn1_bitstring("m", publicKey));
}

/**
 * Form the RSA keyid as a SHA-1 hash of a publicKeyInfo object
 * Also used in rsa_private_key.c.
 */
chunk_t rsa_public_key_id_create(mpz_t n, mpz_t e)
{
	chunk_t keyid;
	chunk_t publicKeyInfo = rsa_public_key_info_to_asn1(n, e);
	hasher_t *hasher = hasher_create(HASH_SHA1);

	hasher->allocate_hash(hasher, publicKeyInfo, &keyid);
	hasher->destroy(hasher);
	free(publicKeyInfo.ptr);

	return keyid;
}

/**
 * Implementation of rsa_public_key_t.get_publicKeyInfo.
 */
static chunk_t get_publicKeyInfo(const private_rsa_public_key_t *this)
{
	return rsa_public_key_info_to_asn1(this->n, this->e);
}

/**
 * Implementation of rsa_public_key_t.get_keyid.
 */
static chunk_t get_keyid(const private_rsa_public_key_t *this)
{
	return this->keyid;
}

/* forward declaration used by rsa_public_key_t.clone */
private_rsa_public_key_t *rsa_public_key_create_empty(void);

/**
 * Implementation of rsa_public_key_t.clone.
 */
static rsa_public_key_t* _clone(const private_rsa_public_key_t *this)
{
	private_rsa_public_key_t *clone = rsa_public_key_create_empty();
	
	mpz_init_set(clone->n, this->n);
	mpz_init_set(clone->e, this->e);
	clone->keyid = chunk_clone(this->keyid);
	clone->k = this->k;
	
	return &clone->public;
}

/**
 * Implementation of rsa_public_key_t.destroy.
 */
static void destroy(private_rsa_public_key_t *this)
{
	mpz_clear(this->n);
	mpz_clear(this->e);
	free(this->keyid.ptr);
	free(this);
}

/**
 * Generic private constructor
 */
private_rsa_public_key_t *rsa_public_key_create_empty(void)
{
	private_rsa_public_key_t *this = malloc_thing(private_rsa_public_key_t);
	
	/* public functions */
	this->public.pkcs1_encrypt = (status_t (*) (rsa_public_key_t*,chunk_t,chunk_t*))pkcs1_encrypt;
	this->public.verify_emsa_pkcs1_signature = (status_t (*) (const rsa_public_key_t*,hash_algorithm_t,chunk_t,chunk_t))verify_emsa_pkcs1_signature;
	this->public.get_modulus = (mpz_t *(*) (const rsa_public_key_t*))get_modulus;
	this->public.get_keysize = (size_t (*) (const rsa_public_key_t*))get_keysize;
	this->public.get_publicKeyInfo = (chunk_t (*) (const rsa_public_key_t*))get_publicKeyInfo;
	this->public.get_keyid = (chunk_t (*) (const rsa_public_key_t*))get_keyid;
	this->public.clone = (rsa_public_key_t* (*) (const rsa_public_key_t*))_clone;
	this->public.destroy = (void (*) (rsa_public_key_t*))destroy;
	
	/* private functions */
	this->rsaep = rsaep;
	this->rsavp1 = rsaep; /* same algorithm */
	
	return this;
}

/*
 * See header
 */
rsa_public_key_t *rsa_public_key_create(mpz_t n, mpz_t e)
{
	private_rsa_public_key_t *this = rsa_public_key_create_empty();

	mpz_init_set(this->n, n);
	mpz_init_set(this->e, e);

	this->k = (mpz_sizeinbase(n, 2) + 7) / BITS_PER_BYTE;
	this->keyid = rsa_public_key_id_create(n, e);
	return &this->public;
}
/*
 * See header
 */
rsa_public_key_t *rsa_public_key_create_from_chunk(chunk_t blob)
{
	asn1_ctx_t ctx;
	chunk_t object;
	u_int level;
	int objectID = 0;

	private_rsa_public_key_t *this = rsa_public_key_create_empty();

	mpz_init(this->n);
	mpz_init(this->e);
	
	asn1_init(&ctx, blob, 0, FALSE, FALSE);
	
	while (objectID < PUB_KEY_ROOF) 
	{
		if (!extract_object(pubkeyObjects, &objectID, &object, &level, &ctx))
		{
			destroy(this);
			return FALSE;
		}
		switch (objectID)
		{
			case PUB_KEY_MODULUS:
				mpz_import(this->n, object.len, 1, 1, 1, 0, object.ptr);
				break;
			case PUB_KEY_EXPONENT:
				mpz_import(this->e, object.len, 1, 1, 1, 0, object.ptr);
				break;
		}
		objectID++;
	}

	this->k = (mpz_sizeinbase(this->n, 2) + 7) / BITS_PER_BYTE;
	this->keyid = rsa_public_key_id_create(this->n, this->e);
	return &this->public;
}

/*
 * See header
 */
rsa_public_key_t *rsa_public_key_create_from_file(char *filename)
{
	bool pgp = FALSE;
	chunk_t chunk = chunk_empty;
	rsa_public_key_t *pubkey = NULL;

	if (!pem_asn1_load_file(filename, NULL, "public key", &chunk, &pgp))
	{
		return NULL;
	}
	pubkey = rsa_public_key_create_from_chunk(chunk);
	free(chunk.ptr);
	return pubkey;
}
