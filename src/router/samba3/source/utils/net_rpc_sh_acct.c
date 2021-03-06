/*
   Samba Unix/Linux SMB client library
   Distributed SMB/CIFS Server Management Utility
   Copyright (C) 2006 Volker Lendecke (vl@samba.org)

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "includes.h"
#include "utils/net.h"

/*
 * Do something with the account policies. Read them all, run a function on
 * them and possibly write them back. "fn" has to return the container index
 * it has modified, it can return 0 for no change.
 */

static NTSTATUS rpc_sh_acct_do(struct net_context *c,
			       TALLOC_CTX *mem_ctx,
			       struct rpc_sh_ctx *ctx,
			       struct rpc_pipe_client *pipe_hnd,
			       int argc, const char **argv,
			       int (*fn)(struct net_context *c,
					  TALLOC_CTX *mem_ctx,
					  struct rpc_sh_ctx *ctx,
					  struct samr_DomInfo1 *i1,
					  struct samr_DomInfo3 *i3,
					  struct samr_DomInfo12 *i12,
					  int argc, const char **argv))
{
	POLICY_HND connect_pol, domain_pol;
	NTSTATUS result = NT_STATUS_UNSUCCESSFUL;
	union samr_DomainInfo *info1 = NULL;
	union samr_DomainInfo *info3 = NULL;
	union samr_DomainInfo *info12 = NULL;
	int store;

	ZERO_STRUCT(connect_pol);
	ZERO_STRUCT(domain_pol);

	/* Get sam policy handle */

	result = rpccli_samr_Connect2(pipe_hnd, mem_ctx,
				      pipe_hnd->desthost,
				      MAXIMUM_ALLOWED_ACCESS,
				      &connect_pol);
	if (!NT_STATUS_IS_OK(result)) {
		goto done;
	}

	/* Get domain policy handle */

	result = rpccli_samr_OpenDomain(pipe_hnd, mem_ctx,
					&connect_pol,
					MAXIMUM_ALLOWED_ACCESS,
					ctx->domain_sid,
					&domain_pol);
	if (!NT_STATUS_IS_OK(result)) {
		goto done;
	}

	result = rpccli_samr_QueryDomainInfo(pipe_hnd, mem_ctx,
					     &domain_pol,
					     1,
					     &info1);

	if (!NT_STATUS_IS_OK(result)) {
		d_fprintf(stderr, "query_domain_info level 1 failed: %s\n",
			  nt_errstr(result));
		goto done;
	}

	result = rpccli_samr_QueryDomainInfo(pipe_hnd, mem_ctx,
					     &domain_pol,
					     3,
					     &info3);

	if (!NT_STATUS_IS_OK(result)) {
		d_fprintf(stderr, "query_domain_info level 3 failed: %s\n",
			  nt_errstr(result));
		goto done;
	}

	result = rpccli_samr_QueryDomainInfo(pipe_hnd, mem_ctx,
					     &domain_pol,
					     12,
					     &info12);

	if (!NT_STATUS_IS_OK(result)) {
		d_fprintf(stderr, "query_domain_info level 12 failed: %s\n",
			  nt_errstr(result));
		goto done;
	}

	store = fn(c, mem_ctx, ctx, &info1->info1, &info3->info3,
		   &info12->info12, argc, argv);

	if (store <= 0) {
		/* Don't save anything */
		goto done;
	}

	switch (store) {
	case 1:
		result = rpccli_samr_SetDomainInfo(pipe_hnd, mem_ctx,
						   &domain_pol,
						   1,
						   info1);
		break;
	case 3:
		result = rpccli_samr_SetDomainInfo(pipe_hnd, mem_ctx,
						   &domain_pol,
						   3,
						   info3);
		break;
	case 12:
		result = rpccli_samr_SetDomainInfo(pipe_hnd, mem_ctx,
						   &domain_pol,
						   12,
						   info12);
		break;
	default:
		d_fprintf(stderr, "Got unexpected info level %d\n", store);
		result = NT_STATUS_INTERNAL_ERROR;
		goto done;
	}

 done:
	if (is_valid_policy_hnd(&domain_pol)) {
		rpccli_samr_Close(pipe_hnd, mem_ctx, &domain_pol);
	}
	if (is_valid_policy_hnd(&connect_pol)) {
		rpccli_samr_Close(pipe_hnd, mem_ctx, &connect_pol);
	}

	return result;
}

static int account_show(struct net_context *c,
			TALLOC_CTX *mem_ctx, struct rpc_sh_ctx *ctx,
			struct samr_DomInfo1 *i1,
			struct samr_DomInfo3 *i3,
			struct samr_DomInfo12 *i12,
			int argc, const char **argv)
{
	if (argc != 0) {
		d_fprintf(stderr, "usage: %s\n", ctx->whoami);
		return -1;
	}

	d_printf("Minimum password length: %d\n", i1->min_password_length);
	d_printf("Password history length: %d\n", i1->password_history_length);

	d_printf("Minimum password age: ");
	if (!nt_time_is_zero((NTTIME *)&i1->min_password_age)) {
		time_t t = nt_time_to_unix_abs((NTTIME *)&i1->min_password_age);
		d_printf("%d seconds\n", (int)t);
	} else {
		d_printf("not set\n");
	}

	d_printf("Maximum password age: ");
	if (nt_time_is_set((NTTIME *)&i1->max_password_age)) {
		time_t t = nt_time_to_unix_abs((NTTIME *)&i1->max_password_age);
		d_printf("%d seconds\n", (int)t);
	} else {
		d_printf("not set\n");
	}

	d_printf("Bad logon attempts: %d\n", i12->lockout_threshold);

	if (i12->lockout_threshold != 0) {

		d_printf("Account lockout duration: ");
		if (nt_time_is_set(&i12->lockout_duration)) {
			time_t t = nt_time_to_unix_abs(&i12->lockout_duration);
			d_printf("%d seconds\n", (int)t);
		} else {
			d_printf("not set\n");
		}

		d_printf("Bad password count reset after: ");
		if (nt_time_is_set(&i12->lockout_window)) {
			time_t t = nt_time_to_unix_abs(&i12->lockout_window);
			d_printf("%d seconds\n", (int)t);
		} else {
			d_printf("not set\n");
		}
	}

	d_printf("Disconnect users when logon hours expire: %s\n",
		 nt_time_is_zero(&i3->force_logoff_time) ? "yes" : "no");

	d_printf("User must logon to change password: %s\n",
		 (i1->password_properties & 0x2) ? "yes" : "no");

	return 0;		/* Don't save */
}

static NTSTATUS rpc_sh_acct_pol_show(struct net_context *c,
				     TALLOC_CTX *mem_ctx,
				     struct rpc_sh_ctx *ctx,
				     struct rpc_pipe_client *pipe_hnd,
				     int argc, const char **argv) {
	return rpc_sh_acct_do(c, mem_ctx, ctx, pipe_hnd, argc, argv,
			      account_show);
}

static int account_set_badpw(struct net_context *c,
			     TALLOC_CTX *mem_ctx, struct rpc_sh_ctx *ctx,
			     struct samr_DomInfo1 *i1,
			     struct samr_DomInfo3 *i3,
			     struct samr_DomInfo12 *i12,
			     int argc, const char **argv)
{
	if (argc != 1) {
		d_fprintf(stderr, "usage: %s <count>\n", ctx->whoami);
		return -1;
	}

	i12->lockout_threshold = atoi(argv[0]);
	d_printf("Setting bad password count to %d\n",
		 i12->lockout_threshold);

	return 12;
}

static NTSTATUS rpc_sh_acct_set_badpw(struct net_context *c,
				      TALLOC_CTX *mem_ctx,
				      struct rpc_sh_ctx *ctx,
				      struct rpc_pipe_client *pipe_hnd,
				      int argc, const char **argv)
{
	return rpc_sh_acct_do(c, mem_ctx, ctx, pipe_hnd, argc, argv,
			      account_set_badpw);
}

static int account_set_lockduration(struct net_context *c,
				    TALLOC_CTX *mem_ctx,
				    struct rpc_sh_ctx *ctx,
				    struct samr_DomInfo1 *i1,
				    struct samr_DomInfo3 *i3,
				    struct samr_DomInfo12 *i12,
				    int argc, const char **argv)
{
	if (argc != 1) {
		d_fprintf(stderr, "usage: %s <count>\n", ctx->whoami);
		return -1;
	}

	unix_to_nt_time_abs(&i12->lockout_duration, atoi(argv[0]));
	d_printf("Setting lockout duration to %d seconds\n",
		 (int)nt_time_to_unix_abs(&i12->lockout_duration));

	return 12;
}

static NTSTATUS rpc_sh_acct_set_lockduration(struct net_context *c,
					     TALLOC_CTX *mem_ctx,
					     struct rpc_sh_ctx *ctx,
					     struct rpc_pipe_client *pipe_hnd,
					     int argc, const char **argv)
{
	return rpc_sh_acct_do(c, mem_ctx, ctx, pipe_hnd, argc, argv,
			      account_set_lockduration);
}

static int account_set_resetduration(struct net_context *c,
				     TALLOC_CTX *mem_ctx,
				     struct rpc_sh_ctx *ctx,
				     struct samr_DomInfo1 *i1,
				     struct samr_DomInfo3 *i3,
				     struct samr_DomInfo12 *i12,
				     int argc, const char **argv)
{
	if (argc != 1) {
		d_fprintf(stderr, "usage: %s <count>\n", ctx->whoami);
		return -1;
	}

	unix_to_nt_time_abs(&i12->lockout_window, atoi(argv[0]));
	d_printf("Setting bad password reset duration to %d seconds\n",
		 (int)nt_time_to_unix_abs(&i12->lockout_window));

	return 12;
}

static NTSTATUS rpc_sh_acct_set_resetduration(struct net_context *c,
					      TALLOC_CTX *mem_ctx,
					      struct rpc_sh_ctx *ctx,
					      struct rpc_pipe_client *pipe_hnd,
					      int argc, const char **argv)
{
	return rpc_sh_acct_do(c, mem_ctx, ctx, pipe_hnd, argc, argv,
			      account_set_resetduration);
}

static int account_set_minpwage(struct net_context *c,
				TALLOC_CTX *mem_ctx,
				struct rpc_sh_ctx *ctx,
				struct samr_DomInfo1 *i1,
				struct samr_DomInfo3 *i3,
				struct samr_DomInfo12 *i12,
				int argc, const char **argv)
{
	if (argc != 1) {
		d_fprintf(stderr, "usage: %s <count>\n", ctx->whoami);
		return -1;
	}

	unix_to_nt_time_abs((NTTIME *)&i1->min_password_age, atoi(argv[0]));
	d_printf("Setting minimum password age to %d seconds\n",
		 (int)nt_time_to_unix_abs((NTTIME *)&i1->min_password_age));

	return 1;
}

static NTSTATUS rpc_sh_acct_set_minpwage(struct net_context *c,
					 TALLOC_CTX *mem_ctx,
					 struct rpc_sh_ctx *ctx,
					 struct rpc_pipe_client *pipe_hnd,
					 int argc, const char **argv)
{
	return rpc_sh_acct_do(c, mem_ctx, ctx, pipe_hnd, argc, argv,
			      account_set_minpwage);
}

static int account_set_maxpwage(struct net_context *c,
				TALLOC_CTX *mem_ctx,
				struct rpc_sh_ctx *ctx,
				struct samr_DomInfo1 *i1,
				struct samr_DomInfo3 *i3,
				struct samr_DomInfo12 *i12,
				int argc, const char **argv)
{
	if (argc != 1) {
		d_fprintf(stderr, "usage: %s <count>\n", ctx->whoami);
		return -1;
	}

	unix_to_nt_time_abs((NTTIME *)&i1->max_password_age, atoi(argv[0]));
	d_printf("Setting maximum password age to %d seconds\n",
		 (int)nt_time_to_unix_abs((NTTIME *)&i1->max_password_age));

	return 1;
}

static NTSTATUS rpc_sh_acct_set_maxpwage(struct net_context *c,
					 TALLOC_CTX *mem_ctx,
					 struct rpc_sh_ctx *ctx,
					 struct rpc_pipe_client *pipe_hnd,
					 int argc, const char **argv)
{
	return rpc_sh_acct_do(c, mem_ctx, ctx, pipe_hnd, argc, argv,
			      account_set_maxpwage);
}

static int account_set_minpwlen(struct net_context *c,
				TALLOC_CTX *mem_ctx,
				struct rpc_sh_ctx *ctx,
				struct samr_DomInfo1 *i1,
				struct samr_DomInfo3 *i3,
				struct samr_DomInfo12 *i12,
				int argc, const char **argv)
{
	if (argc != 1) {
		d_fprintf(stderr, "usage: %s <count>\n", ctx->whoami);
		return -1;
	}

	i1->min_password_length = atoi(argv[0]);
	d_printf("Setting minimum password length to %d\n",
		 i1->min_password_length);

	return 1;
}

static NTSTATUS rpc_sh_acct_set_minpwlen(struct net_context *c,
					 TALLOC_CTX *mem_ctx,
					 struct rpc_sh_ctx *ctx,
					 struct rpc_pipe_client *pipe_hnd,
					 int argc, const char **argv)
{
	return rpc_sh_acct_do(c, mem_ctx, ctx, pipe_hnd, argc, argv,
			      account_set_minpwlen);
}

static int account_set_pwhistlen(struct net_context *c,
				 TALLOC_CTX *mem_ctx,
				 struct rpc_sh_ctx *ctx,
				 struct samr_DomInfo1 *i1,
				 struct samr_DomInfo3 *i3,
				 struct samr_DomInfo12 *i12,
				 int argc, const char **argv)
{
	if (argc != 1) {
		d_fprintf(stderr, "usage: %s <count>\n", ctx->whoami);
		return -1;
	}

	i1->password_history_length = atoi(argv[0]);
	d_printf("Setting password history length to %d\n",
		 i1->password_history_length);

	return 1;
}

static NTSTATUS rpc_sh_acct_set_pwhistlen(struct net_context *c,
					  TALLOC_CTX *mem_ctx,
					  struct rpc_sh_ctx *ctx,
					  struct rpc_pipe_client *pipe_hnd,
					  int argc, const char **argv)
{
	return rpc_sh_acct_do(c, mem_ctx, ctx, pipe_hnd, argc, argv,
			      account_set_pwhistlen);
}

struct rpc_sh_cmd *net_rpc_acct_cmds(struct net_context *c, TALLOC_CTX *mem_ctx,
				     struct rpc_sh_ctx *ctx)
{
	static struct rpc_sh_cmd cmds[9] = {
		{ "show", NULL, &ndr_table_samr.syntax_id, rpc_sh_acct_pol_show,
		  "Show current account policy settings" },
		{ "badpw", NULL, &ndr_table_samr.syntax_id, rpc_sh_acct_set_badpw,
		  "Set bad password count before lockout" },
		{ "lockduration", NULL, &ndr_table_samr.syntax_id, rpc_sh_acct_set_lockduration,
		  "Set account lockout duration" },
		{ "resetduration", NULL, &ndr_table_samr.syntax_id,
		  rpc_sh_acct_set_resetduration,
		  "Set bad password count reset duration" },
		{ "minpwage", NULL, &ndr_table_samr.syntax_id, rpc_sh_acct_set_minpwage,
		  "Set minimum password age" },
		{ "maxpwage", NULL, &ndr_table_samr.syntax_id, rpc_sh_acct_set_maxpwage,
		  "Set maximum password age" },
		{ "minpwlen", NULL, &ndr_table_samr.syntax_id, rpc_sh_acct_set_minpwlen,
		  "Set minimum password length" },
		{ "pwhistlen", NULL, &ndr_table_samr.syntax_id, rpc_sh_acct_set_pwhistlen,
		  "Set the password history length" },
		{ NULL, NULL, 0, NULL, NULL }
	};

	return cmds;
}
