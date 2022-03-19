/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 2.0
 *
 *  This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for more details governing rights and limitations under the License.
 *
 * The Original Code is part of the frlib.
 *
 * The Initial Developer of the Original Code is
 * Frank Reker <frank@reker.net>.
 * Portions created by the Initial Developer are Copyright (C) 2003-2014
 * by the Initial Developer. All Rights Reserved.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef _R__FRLIB_LIB_BASE_ERRORS_H
#define _R__FRLIB_LIB_BASE_ERRORS_H


/*! \file errors.h
 *  \brief Definition of the error codes
 */


#ifdef __cplusplus
extern "C" {
#endif


#define RERR_ISOK(ret)				((ret)>=0)
#define RERR_ISERR(ret)				((ret)<=RERR_FAIL)
#define RERR_BOOLERR(ret)			((ret)<RERR_FALSE)
#define RERR_ISTRUE(ret)			RERR_ISOK(ret)
#define RERR_ISFALSE(ret)			(!RERR_ISTRUE(ret))

#define RERR_OK						0	/*!<\brief ok */
#define RERR_TRUE						0	/*!<\brief true */
#define RERR_FALSE					-1	/*!<\brief false */
#define RERR_EOT						-2	/*!<\brief end of text (not an error) */
#define RERR_REDIRECT				-3	/*!<\brief connection redirected (not an error) */
#define RERR_DELAYED					-4	/*!<\brief operation delayed or scheduled for later operation */
#define RERR_FAIL						-5	/*!<\brief general unspecified error */
#define RERR_NODATA					-6	/*!<\brief no data available */
#define RERR_BREAK					-7	/*!<\brief break was encountered (e.g. on rs232) */
#define RERR_INTERNAL				-8	/*!<\brief internal error - probably an implementation error 
													or a hard to trace back error */
#define RERR_SYSTEM					-9	/*!<\brief error in system call */
#define RERR_PARAM					-10	/*!<\brief invalid parameter */
#define RERR_NOMEM					-11	/*!<\brief out of memory */
#define RERR_NOT_FOUND				-12	/*!<\brief object not found */
#define RERR_TIMEDOUT				-13	/*!<\brief operation timed out */
#define RERR_FORBIDDEN				-14	/*!<\brief operation forbidden */
#define RERR_DISABLED				-15	/*!<\brief requested function disabled */
#define RERR_NOT_SUPPORTED			-16	/*!<\brief requested function not supported or implemented */
#define RERR_NOT_AVAILABLE			-17	/*!<\brief requested resource not available */
#define RERR_NODIR					-18	/*!<\brief not a directory */
#define RERR_CHILD					-19	/*!<\brief some child process produced an error or could not be created */
#define RERR_SCRIPT					-20	/*!<\brief the script child returned a non zero value */
#define RERR_CONFIG					-21	/*!<\brief invalid or missing configuration variable */
#define RERR_AUTH						-22	/*!<\brief authentication failed */
#define RERR_CONNECTION				-23	/*!<\brief connection error */
#define RERR_SSL_CONNECTION		-24	/*!<\brief error in ssl connection */
#define RERR_SERVER					-25	/*!<\brief server error */
#define RERR_UTF8						-26	/*!<\brief invalid utf8 char */
#define RERR_EMPTY_BODY				-27	/*!<\brief mail body empty */
#define RERR_DECRYPTING				-28	/*!<\brief error decrypting */
#define RERR_INVALID_CF				-29	/*!<\brief invalid or missing config file */
#define RERR_INVALID_SCF			-30	/*!<\brief invalid secure config file */
#define RERR_NO_CRYPT_CF			-31	/*!<\brief missing secure config file */
#define RERR_INVALID_DATE			-32	/*!<\brief invalid date */
#define RERR_INVALID_ENTRY			-33	/*!<\brief invalid entry */
#define RERR_INVALID_FILE			-34	/*!<\brief invalid filename or content in invalid format */
#define RERR_INVALID_FORMAT		-35	/*!<\brief invalid format */
#define RERR_INVALID_HOST			-36	/*!<\brief invalid host */
#define RERR_INVALID_IP				-37	/*!<\brief invalid ip */
#define RERR_INVALID_MAIL			-38	/*!<\brief invalid mail */
#define RERR_INVALID_PROT			-39	/*!<\brief invalid protocol */
#define RERR_INVALID_PWD			-40	/*!<\brief invalid password */
#define RERR_INVALID_SIGNATURE	-41	/*!<\brief invalid signature */
#define RERR_INVALID_SPOOL			-42	/*!<\brief invalid spool file */
#define RERR_INVALID_TYPE			-43	/*!<\brief invalid type */
#define RERR_INVALID_URL			-44	/*!<\brief invalid url */
#define RERR_INVALID_REPVAR		-45	/*!<\brief invalid repository variable */
#define RERR_INVALID_NAME			-46	/*!<\brief invalid name */
#define RERR_INVALID_SHM			-47	/*!<\brief invalid shared memory page */
#define RERR_INVALID_VAL			-48	/*!<\brief invalid value */
#define RERR_INVALID_VAR			-49	/*!<\brief invalid variable */
#define RERR_INVALID_CMD			-50	/*!<\brief invalid command */
#define RERR_INVALID_CHAN			-51	/*!<\brief invalid channel */
#define RERR_INVALID_PATH			-52	/*!<\brief invalid path */
#define RERR_INVALID_PAGE			-53	/*!<\brief invalid page */
#define RERR_INVALID_LEN			-54	/*!<\brief invalid length */
#define RERR_INVALID_TAG			-55	/*!<\brief invalid tag */
#define RERR_INVALID_TZ				-56	/*!<\brief invalid timezone */
#define RERR_INVALID_ID				-57	/*!<\brief invalid identifier */
#define RERR_INVALID_MODE			-58	/*!<\brief invalid mode */
#define RERR_INVALID_PARAM			-59	/*!<\brief invalid parameter in user input */
#define RERR_LOOP						-60	/*!<\brief loop encountered */
#define RERR_MAIL_NOT_ENCRYPTED	-61	/*!<\brief mail not encrypted */
#define RERR_MAIL_NOT_SIGNED		-62	/*!<\brief mail not signed */
#define RERR_NO_VALID_ATTACH		-63	/*!<\brief mail has no valid attachment */
#define RERR_NOIPV4					-64	/*!<\brief IPv6 is not a mapped IPv4 */
#define RERR_NOKEY					-65	/*!<\brief invalid key value */
#define RERR_NOT_BASE64				-66	/*!<\brief not in base64 format */
#define RERR_NO_XML					-67	/*!<\brief not an xml */
#define RERR_REGISTRY				-68	/*!<\brief problems accessing the windows registry */
#define RERR_OUTOFRANGE				-69	/*!<\brief value out of range */
#define RERR_NOTINIT					-70	/*!<\brief value not initialized yet */
#define RERR_ALREADY_OPEN			-71	/*!<\brief object is already open */
#define RERR_ALREADY_EXIST			-72	/*!<\brief object does already exist */
#define RERR_LOCKED					-73	/*!<\brief object is locked */
#define RERR_FULL						-74	/*!<\brief object is full */
#define RERR_INSUFFICIENT_DATA	-75	/*!<\brief has insufficient data to complete task */
#define RERR_TOO_LONG				-76	/*!<\brief object too long */
#define RERR_TOO_MANY_HOPS			-77	/*!<\brief too many hops encountered */
#define RERR_STACK_OVERFLOW		-78	/*!<\brief stack overflow */
#define RERR_NOT_UNIQ				-79	/*!<\brief object is not unique */
#define RERR_SYNTAX					-80	/*!<\brief syntax error in user input */
#define RERR_PROTOCOL				-81	/*!<\brief protocol error */
#define RERR_CHKSUM					-82	/*!<\brief checksum error */
#define RERR_VERSION					-83	/*!<\brief invalid version number */
#define RERR_BUSY						-84	/*!<\brief system is busy */
#define RERR_TRUNCATED				-85	/*!<\brief write truncated */
#define RERR_MAX						-85	/*!<\brief maximum error code */
#define RERR_OFFSET					(RERR_MAX-10)	/*!<\brief use this as base for your own error codes */




const char *rerr_getstr (int myerrno);
char *rerr_getstr2  (char *outstr, int outlen, int myerrno);
char *rerr_getstr2s (int myerrno);
char *rerr_getstr2m (int myerrno);
#define rerr_getstr3(myerrno)	(rerr_getstr2(_r__errstr, sizeof(_r__errstr), (myerrno)))

const char *rerr_getname (int err);
int rerr_mapname (const char *errname);


struct rerrmap {
	int			rerrno;
	const char	*rerrname;
	const char	*rerrstr;
};

int rerr_register (struct rerrmap*);
int rerr_unregister (struct rerrmap*);

int rerr_regspecialmap (int from, int to, const char* (*func)(int), const char *prefix);

#ifdef __cplusplus
}	/* extern "C" */
#endif







#endif	/* _R__FRLIB_LIB_BASE_ERRORS_H */

/*
 * Overrides for XEmacs and vim so that we get a uniform tabbing style.
 * XEmacs/vim will notice this stuff at the end of the file and automatically
 * adjust the settings for this buffer only.  This must remain at the end
 * of the file.
 * ---------------------------------------------------------------------------
 * Local variables:
 * c-indent-level: 3
 * c-basic-offset: 3
 * tab-width: 3
 * End:
 * vim:tw=0:ts=3:wm=0:
 */
