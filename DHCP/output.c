/*
 * The dhcpd-pools has BSD 2-clause license which also known as "Simplified
 * BSD License" or "FreeBSD License".
 *
 * Copyright 2006- Sami Kerola. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *    1. Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the
 *       distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR AND CONTRIBUTORS OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation are
 * those of the authors and should not be interpreted as representing
 * official policies, either expressed or implied, of Sami Kerola.
 */

/*! \file output.c
 * \brief All about output formats.
 */

#include <config.h>

#include <arpa/inet.h>
#include <errno.h>
#include <inttypes.h>
#include <langinfo.h>
#include <locale.h>
#include <math.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "close-stream.h"
#include "error.h"
#include "progname.h"
#include "strftime.h"

#include "dhcpd-pools.h"

/*! \enum colored_formats
 * \brief Enumeration of output formats.  Keep the text and html first, they
 * are used color array selector.
 */
enum colored_formats {
	OUT_FORM_TEXT,
	OUT_FORM_HTML,
	NUM_OF_OUT_FORMS
};

/*! \enum count_status_t
 * \brief Enumeration of possible range and shared net statuses.
 */
enum count_status_t {
	STATUS_OK,
	STATUS_WARN,
	STATUS_CRIT,
	STATUS_IGNORED,
	STATUS_SUPPRESSED,
	COLOR_RESET
};

/*! \var color_tags
 * \brief Array of stings that make colors to start and end in different
 * schemas per array column. */
static const char *color_tags[][NUM_OF_OUT_FORMS] = {
	[STATUS_OK]	    = { "",		"" },
	[STATUS_WARN]	    = { "\033[1;33m",	" style=\"color:magenta;font-style:italic\"" },
	[STATUS_CRIT]	    = { "\033[1;31m",	" style=\"color:red;font-weight:bold\"" },
	[STATUS_IGNORED]    = { "\033[1;32m",	" style=\"color:green\"" },
	[STATUS_SUPPRESSED] = { "\033[1;34m",	" style=\"color:blue\"" },
	[COLOR_RESET]	    = { "\033[0m",	"" }
};

/*! \brief Calculate range percentages and such.
 * \return Indicator if the entry should be skipped from output. */
int range_output_helper(struct conf_t *state, struct output_helper_t *oh,
			struct range_t *range_p)
{
	/* counts and calculations */
	oh->range_size = get_range_size(range_p);
	oh->percent = (double)(100 * range_p->count) / oh->range_size;
	oh->tc = range_p->touched + range_p->count;
	oh->tcp = (double)(100 * oh->tc) / oh->range_size;
	if (state->backups_found == 1) {
		oh->bup = (double)(100 * range_p->backups) / oh->range_size;
	}
	/* set status */
	oh->status = STATUS_OK;
	if (state->critical < oh->percent && (oh->range_size - range_p->count) < state->crit_count)
		oh->status = STATUS_CRIT;
	else if (state->warning < oh->percent
		 && (oh->range_size - range_p->count) < state->warn_count)
		oh->status = STATUS_WARN;
	if (oh->status != STATUS_OK) {
		if (oh->range_size <= state->minsize) {
			oh->status = STATUS_IGNORED;
			if (state->skip_minsize)
				return 1;
		} else if (state->snet_alarms && range_p->shared_net != state->shared_net_root) {
			oh->status = STATUS_SUPPRESSED;
			if (state->skip_suppressed)
				return 1;
		}
	}
	if ((state->skip_ok && oh->status == STATUS_OK) ||
	    (state->skip_warning && oh->status == STATUS_WARN) ||
	    (state->skip_critical && oh->status == STATUS_CRIT))
		return 1;
	return 0;
}

/*! \brief Calculate shared network percentages and such.
 * \return Indicator if the entry should be skipped from output. */
int shnet_output_helper(struct conf_t *state, struct output_helper_t *oh,
			struct shared_network_t *shared_p)
{
	/* counts and calculations */
	oh->tc = shared_p->touched + shared_p->used;
	if (fpclassify(shared_p->available) == FP_ZERO) {
		oh->percent = NAN;
		oh->tcp = NAN;
		oh->bup = NAN;
		oh->status = STATUS_SUPPRESSED;
		if (state->skip_suppressed)
			return 1;
		return 0;
	}

	oh->percent = (double)(100 * shared_p->used) / shared_p->available;
	oh->tcp = (double)((100 * (shared_p->touched + shared_p->used)) / shared_p->available);
	if (state->backups_found == 1)
		oh->bup = (double)(100 * shared_p->backups) / shared_p->available;

	/* set status */
	if (shared_p->available <= state->minsize) {
		oh->status = STATUS_IGNORED;
		if (state->skip_minsize)
			return 1;
	} else if (state->critical < oh->percent && shared_p->used < state->crit_count) {
		oh->status = STATUS_CRIT;
		if (state->skip_critical)
			return 1;
	} else if (state->warning < oh->percent && shared_p->used < state->warn_count) {
		oh->status = STATUS_WARN;
		if (state->skip_warning)
			return 1;
	} else {
		oh->status = STATUS_OK;
		if (state->skip_ok)
			return 1;
	}
	return 0;

}

/*! \brief Output a color based on output_helper_t status.
 * \return Indicator whether coloring was started or not. */
static int start_color(struct conf_t *state, struct output_helper_t *oh, FILE *outfile)
{
	if (oh->status == STATUS_OK) {
		return 0;
	}
	fputs(color_tags[oh->status][state->output_format], outfile);
	return 1;
}

/*! \brief Helper function to open a output file.
 * \return The outfile in all of the output functions. */
static FILE *open_outfile(struct conf_t *state)
{
	FILE *outfile;

	if (state->output_file) {
		outfile = fopen(state->output_file, "w+");
		if (outfile == NULL) {
			error(EXIT_FAILURE, errno, "open_outfile: %s", state->output_file);
		}
	} else {
		outfile = stdout;
	}
	return outfile;
}


/*! \brief Helper function to close outfile. */
static void close_outfile(FILE *outfile)
{
	if (outfile == stdout) {
		if (fflush(stdout))
			error(EXIT_FAILURE, errno, "close_outfile: fflush");
	} else {
		if (close_stream(outfile))
			error(EXIT_FAILURE, errno, "close_outfile: fclose");
	}
}

/*! \brief Text output format, which is the default. */
static int output_txt(struct conf_t *state)
{
	unsigned int i;
	struct range_t *range_p;
	struct shared_network_t *shared_p;
	struct output_helper_t oh;
	FILE *outfile;
	int max_ipaddr_length = state->ip_version == IPv6 ? 39 : 16;

	if (state->color_mode == color_auto && isatty(STDIN_FILENO)) {
		state->color_mode = color_on;
	}

	outfile = open_outfile(state);
	range_p = state->ranges;

	if (state->header_limit & R_BIT) {
		fprintf(outfile, "Ranges:\n");
		fprintf
		    (outfile,
		     "%-20s%-*s   %-*s %5s %5s %10s  %5s %5s %9s",
		     "shared net name",
		     max_ipaddr_length,
		     "first ip",
		     max_ipaddr_length,
		     "last ip", "max", "cur", "percent", "touch", "t+c", "t+c perc");
		if (state->backups_found == 1) {
			fprintf(outfile, "     bu  bu perc");
		}
		fprintf(outfile, "\n");
	}
	if (state->number_limit & R_BIT) {
		for (i = 0; i < state->num_ranges; i++) {
			int color_set = 0;

			if (range_output_helper(state, &oh, range_p)) {
				range_p++;
				continue;
			}
			if (state->color_mode == color_on)
				color_set = start_color(state, &oh, outfile);
			if (range_p->shared_net) {
				fprintf(outfile, "%-20s", range_p->shared_net->name);
			} else {
				fprintf(outfile, "not_defined         ");
			}
			/* Outputting of first_ip and last_ip need to be
			 * separate since ntop_ipaddr always returns the
			 * same buffer */
			fprintf(outfile, "%-*s",
				max_ipaddr_length, ntop_ipaddr(&range_p->first_ip));
			fprintf(outfile,
				" - %-*s %5g %5g %10.3f  %5g %5g %9.3f",
				max_ipaddr_length,
				ntop_ipaddr(&range_p->last_ip),
				oh.range_size,
				range_p->count,
				oh.percent,
				range_p->touched,
				oh.tc,
				oh.tcp);
			if (state->backups_found == 1) {
				fprintf(outfile, "%7g %8.3f", range_p->backups, oh.bup);
			}
			if (color_set)
				fputs(color_tags[COLOR_RESET][state->output_format], outfile);
			fprintf(outfile, "\n");
			range_p++;
		}
	}
	if (state->number_limit & R_BIT && state->header_limit & S_BIT) {
		fprintf(outfile, "\n");
	}
	if (state->header_limit & S_BIT) {
		fprintf(outfile, "Shared networks:\n");
		fprintf(outfile,
			"name                   max   cur     percent  touch    t+c  t+c perc");
		if (state->backups_found == 1) {
			fprintf(outfile, "     bu  bu perc");
		}
		fprintf(outfile, "\n");
	}
	if (state->number_limit & S_BIT) {
		for (shared_p = state->shared_net_root->next; shared_p; shared_p = shared_p->next) {
			int color_set = 0;

			if (shnet_output_helper(state, &oh, shared_p))
				continue;
			if (state->color_mode == color_on)
				color_set = start_color(state, &oh, outfile);
			fprintf(outfile,
				"%-20s %5g %5g %10.3f %7g %6g %9.3f",
				shared_p->name,
				shared_p->available,
				shared_p->used,
				oh.percent,
				shared_p->touched,
				oh.tc,
				oh.tcp);
			if (state->backups_found == 1) {
				fprintf(outfile, "%7g %8.3f", shared_p->backups, oh.bup);
			}
			if (color_set)
				fputs(color_tags[COLOR_RESET][state->output_format], outfile);
			fprintf(outfile, "\n");
		}
	}
	if (state->number_limit & S_BIT && state->header_limit & A_BIT) {
		fprintf(outfile, "\n");
	}
	if (state->header_limit & A_BIT) {
		fprintf(outfile, "Sum of all ranges:\n");
		fprintf(outfile,
			"name                   max   cur     percent  touch    t+c  t+c perc");

		if (state->backups_found == 1) {
			fprintf(outfile, "     bu  bu perc");
		}
		fprintf(outfile, "\n");
	}
	if (state->number_limit & A_BIT) {
		int color_set = 0;

		shnet_output_helper(state, &oh, state->shared_net_root);
		if (state->color_mode == color_on)
			color_set = start_color(state, &oh, outfile);
		fprintf(outfile, "%-20s %5g %5g %10.3f %7g %6g %9.3f",
			state->shared_net_root->name,
			state->shared_net_root->available,
			state->shared_net_root->used,
			oh.percent,
			state->shared_net_root->touched,
			oh.tc,
			oh.tcp);

		if (state->backups_found == 1) {
			fprintf(outfile, "%7g %8.3f", state->shared_net_root->backups, oh.bup);
		}
		if (color_set)
			fputs(color_tags[COLOR_RESET][state->output_format], outfile);
		fprintf(outfile, "\n");
	}
	close_outfile(outfile);
	return 0;
}

/*! \brief The xml output formats. */
static int output_xml(struct conf_t *state, const int print_mac_addreses)
{
	unsigned int i;
	struct range_t *range_p;
	struct shared_network_t *shared_p;
	struct output_helper_t oh;
	FILE *outfile;

	outfile = open_outfile(state);
	range_p = state->ranges;

	fprintf(outfile, "<dhcpstatus>\n");

	if (print_mac_addreses == 1) {
		struct leases_t *l;

		for (l = state->leases; l != NULL; l = l->hh.next) {
			if (l->type == ACTIVE) {
				fputs("<active_lease>\n\t<ip>", outfile);
				fputs(ntop_ipaddr(&l->ip), outfile);
				fputs("</ip>\n\t<macaddress>", outfile);
				if (l->ethernet != NULL) {
					fputs(l->ethernet, outfile);
				}
				fputs("</macaddress>\n</active_lease>\n", outfile);
			}
		}
	}

	if (state->number_limit & R_BIT) {
		for (i = 0; i < state->num_ranges; i++) {
			if (range_output_helper(state, &oh, range_p)) {
				range_p++;
				continue;
			}
			fprintf(outfile, "<subnet>\n");
			if (range_p->shared_net) {
				fprintf(outfile,
					"\t<location>%s</location>\n", range_p->shared_net->name);
			} else {
				fprintf(outfile, "\t<location></location>\n");
			}
			fprintf(outfile, "\t<range>%s ", ntop_ipaddr(&range_p->first_ip));
			fprintf(outfile, "- %s</range>\n", ntop_ipaddr(&range_p->last_ip));
			fprintf(outfile, "\t<defined>%g</defined>\n", oh.range_size);
			fprintf(outfile, "\t<used>%g</used>\n", range_p->count);
			fprintf(outfile, "\t<touched>%g</touched>\n", range_p->touched);
			fprintf(outfile, "\t<free>%g</free>\n", oh.range_size - range_p->count);
			range_p++;
			fprintf(outfile, "</subnet>\n");
		}
	}

	if (state->number_limit & S_BIT) {
		for (shared_p = state->shared_net_root->next; shared_p; shared_p = shared_p->next) {
			if (shnet_output_helper(state, &oh, shared_p))
				continue;
			fprintf(outfile, "<shared-network>\n");
			fprintf(outfile, "\t<location>%s</location>\n", shared_p->name);
			fprintf(outfile, "\t<defined>%g</defined>\n", shared_p->available);
			fprintf(outfile, "\t<used>%g</used>\n", shared_p->used);
			fprintf(outfile, "\t<touched>%g</touched>\n", shared_p->touched);
			fprintf(outfile, "\t<free>%g</free>\n",
				shared_p->available - shared_p->used);
			fprintf(outfile, "</shared-network>\n");
		}
	}

	if (state->header_limit & A_BIT) {
		fprintf(outfile, "<summary>\n");
		fprintf(outfile, "\t<location>%s</location>\n", state->shared_net_root->name);
		fprintf(outfile, "\t<defined>%g</defined>\n", state->shared_net_root->available);
		fprintf(outfile, "\t<used>%g</used>\n", state->shared_net_root->used);
		fprintf(outfile, "\t<touched>%g</touched>\n", state->shared_net_root->touched);
		fprintf(outfile, "\t<free>%g</free>\n",
			state->shared_net_root->available - state->shared_net_root->used);
		fprintf(outfile, "</summary>\n");
	}

	fprintf(outfile, "</dhcpstatus>\n");
	close_outfile(outfile);
	return 0;
}

/*! \brief The json output formats. */
static int output_json(struct conf_t *state, const int print_mac_addreses)
{
	unsigned int i = 0;
	struct range_t *range_p;
	struct shared_network_t *shared_p;
	struct output_helper_t oh;
	FILE *outfile;
	unsigned int sep;

	outfile = open_outfile(state);
	range_p = state->ranges;
	sep = 0;

	fprintf(outfile, "{\n");

	if (print_mac_addreses == 1) {
		struct leases_t *l;

		fprintf(outfile, "   \"active_leases\": [");
		for (l = state->leases; l != NULL; l = l->hh.next) {
			if (l->type == ACTIVE) {
				if (i == 0) {
					i = 1;
				} else {
					fputc(',', outfile);
				}
				fputs("\n         { \"ip\":\"", outfile);
				fputs(ntop_ipaddr(&l->ip), outfile);
				fputs("\", \"macaddress\":\"", outfile);
				if (l->ethernet != NULL) {
					fputs(l->ethernet, outfile);
				}
				fputs("\" }", outfile);
			}
		}
		fprintf(outfile, "\n   ]");	/* end of active_leases */
		sep++;
	}

	if (state->number_limit & R_BIT) {
		if (sep) {
			fprintf(outfile, ",\n");
		}
		fprintf(outfile, "   \"subnets\": [\n");
		for (i = 0; i < state->num_ranges; i++) {
			if (range_output_helper(state, &oh, range_p)) {
				range_p++;
				continue;
			}
			fprintf(outfile, "         ");
			fprintf(outfile, "{ ");
			if (range_p->shared_net) {
				fprintf(outfile,
					"\"location\":\"%s\", ", range_p->shared_net->name);
			} else {
				fprintf(outfile, "\"location\":\"\", ");
			}

			fprintf(outfile, "\"range\":\"%s", ntop_ipaddr(&range_p->first_ip));
			fprintf(outfile, " - %s\", ", ntop_ipaddr(&range_p->last_ip));
			fprintf(outfile, "\"first_ip\":\"%s\", ", ntop_ipaddr(&range_p->first_ip));
			fprintf(outfile, "\"last_ip\":\"%s\", ", ntop_ipaddr(&range_p->last_ip));
			fprintf(outfile, "\"defined\":%g, ", oh.range_size);
			fprintf(outfile, "\"used\":%g, ", range_p->count);
			fprintf(outfile, "\"touched\":%g, ", range_p->touched);
			fprintf(outfile, "\"free\":%g, ", oh.range_size - range_p->count);
			fprintf(outfile, "\"percent\":%g, ", oh.percent);
			fprintf(outfile, "\"touch_count\":%g, ", oh.tc);
			fprintf(outfile, "\"touch_percent\":%g, ", oh.tcp);
			if (state->backups_found == 1) {
				fprintf(outfile, "\"backup_count\":%g, ", range_p->backups);
				fprintf(outfile, "\"backup_percent\":%g, ", oh.bup);
			}
			fprintf(outfile, "\"status\":%d ", oh.status);

			range_p++;
			if (i + 1 < state->num_ranges)
				fprintf(outfile, "},\n");
			else
				fprintf(outfile, "}\n");
		}
		fprintf(outfile, "   ]");	/* end of subnets */
		sep++;
	}

	if (state->number_limit & S_BIT) {
		if (sep) {
			fprintf(outfile, ",\n");
		}
		fprintf(outfile, "   \"shared-networks\": [\n");
		for (shared_p = state->shared_net_root->next; shared_p; shared_p = shared_p->next) {
			if (shnet_output_helper(state, &oh, shared_p))
				continue;
			fprintf(outfile, "         ");
			fprintf(outfile, "{ ");
			fprintf(outfile, "\"location\":\"%s\", ", shared_p->name);
			fprintf(outfile, "\"defined\":%g, ", shared_p->available);
			fprintf(outfile, "\"used\":%g, ", shared_p->used);
			fprintf(outfile, "\"touched\":%g, ", shared_p->touched);
			fprintf(outfile, "\"free\":%g, ", shared_p->available - shared_p->used);
			if (fpclassify(shared_p->available) == FP_ZERO)
				fprintf(outfile, "\"percent\":\"%g\", ", oh.percent);
			else
				fprintf(outfile, "\"percent\":%g, ", oh.percent);
			fprintf(outfile, "\"touch_count\":%g, ", oh.tc);
			if (fpclassify(shared_p->available) == FP_ZERO)
				fprintf(outfile, "\"touch_percent\":\"%g\", ", oh.tcp);
			else
				fprintf(outfile, "\"touch_percent\":%g, ", oh.tcp);
			if (state->backups_found == 1) {
				fprintf(outfile, "\"backup_count\":%g, ", shared_p->backups);
				if (fpclassify(shared_p->available) == FP_ZERO)
					fprintf(outfile, "\"backup_percent\":\"%g\", ", oh.bup);
				else
					fprintf(outfile, "\"backup_percent\":%g, ", oh.bup);
			}
			fprintf(outfile, "\"status\":%d ", oh.status);
			if (shared_p->next)
				fprintf(outfile, "},\n");
			else
				fprintf(outfile, "}\n");
		}
		fprintf(outfile, "   ]");	/* end of shared-networks */
		sep++;
	}

	if (state->header_limit & A_BIT) {
		shnet_output_helper(state, &oh, state->shared_net_root);
		if (sep) {
			fprintf(outfile, ",\n");
		}
		fprintf(outfile, "   \"summary\": {\n");
		fprintf(outfile, "         \"location\":\"%s\",\n", state->shared_net_root->name);
		fprintf(outfile, "         \"defined\":%g,\n", state->shared_net_root->available);
		fprintf(outfile, "         \"used\":%g,\n", state->shared_net_root->used);
		fprintf(outfile, "         \"touched\":%g,\n", state->shared_net_root->touched);
		fprintf(outfile, "         \"free\":%g,\n",
			state->shared_net_root->available - state->shared_net_root->used);
		fprintf(outfile, "         \"percent\":%g,\n", oh.percent);
		fprintf(outfile, "         \"touch_count\":%g,\n", oh.tc);
		fprintf(outfile, "         \"touch_percent\":%g,\n", oh.tcp);
		if (state->backups_found == 1) {
			fprintf(outfile, "         \"backup_count\":%g,\n",
				state->shared_net_root->backups);
			fprintf(outfile, "         \"backup_percent\":%g,\n", oh.bup);
		}
		fprintf(outfile, "         \"status\":%d\n", oh.status);
		fprintf(outfile, "   },\n");	/* end of summary */
		fprintf(outfile, "   \"trivia\": {\n");
		fprintf(outfile, "         \"version\":\"%s\",\n", PACKAGE_VERSION);
		fprintf(outfile, "         \"conf_file_path\":\"%s\",\n", state->dhcpdconf_file);
		fprintf(outfile, "         \"conf_file_epoch_mtime\":");
		dp_time_tool(outfile, state->dhcpdconf_file, 1);
		fprintf(outfile, ",\n");
		fprintf(outfile, "         \"lease_file_path\":\"%s\",\n", state->dhcpdlease_file);
		fprintf(outfile, "         \"lease_file_epoch_mtime\":");
		dp_time_tool(outfile, state->dhcpdlease_file, 1);
		fprintf(outfile, "\n");

		fprintf(outfile, "   }");	/* end of trivia */
	}
	fprintf(outfile, "\n}\n");
	close_outfile(outfile);
	return 0;
}

/*! \brief Header for full html output format.
 *
 * \param f Output file descriptor.
 */
static void html_header(struct conf_t *state, FILE *restrict f)
{
	fprintf(f, "<!DOCTYPE html>\n");
	fprintf(f, "<html>\n");
	fprintf(f, "<head>\n");
	fprintf(f, "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\" />\n");
	fprintf(f, "<title>ISC dhcpd 地址分配状态</title>\n");
	fprintf(f, "<meta http-equiv=\"X-UA-Compatible\" content=\"IE=edge\">\n");
	fprintf(f, "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n");
	fprintf(f, "<script src=\"/inc/jquery.min.js\" type=\"text/javascript\"></script>\n");
	//fprintf(f, "<link rel=\"stylesheet\" href=\"https://maxcdn.bootstrapcdn.com/bootstrap/3.3.7/css/bootstrap.min.css\" type=\"text/css\">\n");
	//fprintf(f, "<link rel=\"stylesheet\" type=\"text/css\" href=\"https://cdn.datatables.net/v/bs/jq-3.2.1/dt-1.10.16/datatables.min.css\">\n");
	fprintf(f, "<link rel=\"stylesheet\" href=\"/inc/bootstrap.min.css\" type=\"text/css\">\n");
	fprintf(f, "<link rel=\"stylesheet\" type=\"text/css\" href=\"/inc/datatables.min.css\">\n");
	fprintf(f, "<style type=\"text/css\">\n");
	fprintf(f, "table.dhcpd-pools th { text-transform: capitalize }\n");
	fprintf(f, "</style>\n");
	fprintf(f, "</head>\n");
	fprintf(f, "<body>\n");
	fprintf(f, "<div class=\"container\">\n");
	fprintf(f, "<h2>ISC DHCPD状态</h2>\n");
	fprintf(f, "<small>文件 %s 最后修改时间 ", state->dhcpdlease_file);
	dp_time_tool(f, state->dhcpdlease_file, 0);
	fprintf(f, "</small><hr />\n");
}

/*! \brief Footer for full html output format.
 *
 * \param f Output file descriptor.
 */
static void html_footer(FILE *restrict f)
{
	fprintf(f, "<br /><div class=\"well well-lg\">\n");
	fprintf(f, "<small>本页面由 %s 生成<br />\n", PACKAGE_STRING);
	fprintf(f, "更多信息请参见 <a href=\"%s\">%s</a>\n", PACKAGE_URL, PACKAGE_URL);
	fprintf(f, "</small></div></div>\n");
	//fprintf(f, "<script src=\"https://maxcdn.bootstrapcdn.com/bootstrap/3.3.7/js/bootstrap.min.js\" type=\"text/javascript\"></script>\n");
	//fprintf(f, "<script type=\"text/javascript\" src=\"https://cdn.datatables.net/v/bs/jq-3.2.1/dt-1.10.16/datatables.min.js\"></script>\n");
	fprintf(f, "<script src=\"/inc/bootstrap.min.js\" type=\"text/javascript\"></script>\n");
	fprintf(f, "<script type=\"text/javascript\" src=\"/inc/datatables.min.js\"></script>\n");
	fprintf(f, "<script type=\"text/javascript\" class=\"init\">$(document).ready(function() { $('#s').DataTable({ \"iDisplayLength\": 50, \"lengthMenu\": [ [25, 50, 100, -1], [25, 50, 100, \"All\"] ], \"order\": [[ 4, \"desc\" ]] } ); } );</script>\n");
	fprintf(f, "<script type=\"text/javascript\" class=\"init\">$(document).ready(function() { $('#r').DataTable({ \"iDisplayLength\": 100, \"lengthMenu\": [ [25, 50, 100, -1], [25, 50, 100, \"All\"] ], \"order\": [[ 6, \"desc\" ]] } ); } );</script>\n");
	fprintf(f, "</body></html>\n");
}

/*! \brief Start a html tag.
 *
 * \param f Output file descriptor.
 * \param tag The html tag.
 */
static void start_tag(FILE *restrict f, char const *restrict tag)
{
	fprintf(f, "<%s>\n", tag);
}

/*! \brief End a html tag.
 *
 * \param f Output file descriptor.
 * \param tag The html tag.
 */
static void end_tag(FILE *restrict f, char const *restrict tag)
{
	fprintf(f, "</%s>\n", tag);
}

/*! \brief Line with text in html output format.
 *
 * \param f Output file descriptor.
 * \param type HTML tag name.
 * \param class How the data is aligned.
 * \param text Actual payload of the printout.
 */
static void output_line(FILE *restrict f, char const *restrict type, char const *restrict text)
{
	fprintf(f, "<%s>%s</%s>\n", type, text, type);
}

/*! \brief Line with digit in html output format.
 *
 * \param f Output file descriptor.
 * \param type HMTL tag name.
 * \param d Actual payload of the printout.
 */
static void output_double(FILE *restrict f, char const *restrict type, double d)
{
	fprintf(f, "<%s>%g</%s>\n", type, d, type);
}

/*! \brief Line with a potentially colored digit in html output format.
 *
 * \param state Runtime configuration state.
 * \param f Output file descriptor.
 * \param type HMTL tag name.
 * \param d Actual payload of the printout.
 */
static void output_double_color(struct conf_t *state,
				struct output_helper_t *oh, FILE *restrict f,
				char const *restrict type)
{
	fprintf(f, "<%s", type);
	if (state->color_mode == color_on)
		start_color(state, oh, f);
	fprintf(f, ">%g", oh->percent);
	fprintf(f, "</%s>\n", type);
}

/*! \brief Line with float in html output format.
 *
 * \param f Output file descriptor.
 * \param type HTML tag name.
 * \param fl Actual payload of the printout.
 */
static void output_float(FILE *restrict f, char const *restrict type, float fl)
{
	fprintf(f, "<%s>%.3f</%s>\n", type, fl, type);
}

/*! \brief Begin table in html output format.
 *
 * \param f Output file descriptor.
 */
static void table_start(FILE *restrict f, char const *restrict id, char const *restrict summary)
{
	fprintf(f, "<table id=\"%s\" class=\"dhcpd-pools order-column table table-hover\" summary=\"%s\">\n", id, summary);
}

/*! \brief End table in html output format.
 *
 * \param f Output file descriptor.
 */
static void table_end(FILE *restrict f)
{
	fprintf(f, "</table>\n");
}

/*! \brief New section in html output format.
 *
 * \param f Output file descriptor.
 * \param title Table title.
 */
static void newsection(FILE *restrict f, char const *restrict title)
{
	output_line(f, "h3", title);
}

/*! \brief Output html format. */
static int output_html(struct conf_t *state)
{
	unsigned int i;
	struct range_t *range_p;
	struct shared_network_t *shared_p;
	struct output_helper_t oh;
	FILE *outfile;

	outfile = open_outfile(state);
	range_p = state->ranges;
	html_header(state, outfile);
	newsection(outfile, "汇总信息");
	table_start(outfile, "a", "all");
	if (state->header_limit & A_BIT) {
		start_tag(outfile, "thead");
		start_tag(outfile, "tr");
		output_line(outfile, "th", "名称");
		output_line(outfile, "th", "总数");
		output_line(outfile, "th", "在用");
		output_line(outfile, "th", "空闲");
		output_line(outfile, "th", "比例");
		output_line(outfile, "th", "曾用");
		output_line(outfile, "th", "在用+曾用");
		output_line(outfile, "th", "在用+曾用比例");
		if (state->backups_found == 1) {
			output_line(outfile, "th", "bu");
			output_line(outfile, "th", "bu perc");
		}
		end_tag(outfile, "tr");
		end_tag(outfile, "thead");
	}
	if (state->number_limit & A_BIT) {
		start_tag(outfile, "tbody");
		start_tag(outfile, "tr");
		shnet_output_helper(state, &oh, state->shared_net_root);
		output_line(outfile, "td", state->shared_net_root->name);
		output_double(outfile, "td", state->shared_net_root->available);
		output_double(outfile, "td", state->shared_net_root->used);
		output_double(outfile, "td", state->shared_net_root->available - state->shared_net_root->used);
		output_float(outfile, "td", oh.percent);
		output_double(outfile, "td", state->shared_net_root->touched);
		output_double(outfile, "td", oh.tc);
		output_float(outfile, "td", oh.tcp);
		if (state->backups_found == 1) {
			output_double(outfile, "td", state->shared_net_root->backups);
			output_float(outfile, "td", oh.tcp);
		}
		end_tag(outfile, "tr");
		end_tag(outfile, "tbody");
	}
	table_end(outfile);
	/*newsection(outfile, "多地址段网络");
	table_start(outfile, "s", "snet");
	if (state->header_limit & S_BIT) {
		start_tag(outfile, "thead");
		start_tag(outfile, "tr");
		output_line(outfile, "th", "名称");
		output_line(outfile, "th", "总数");
		output_line(outfile, "th", "在用");
		output_line(outfile, "th", "空闲");
		output_line(outfile, "th", "比例");
		output_line(outfile, "th", "曾用");
		output_line(outfile, "th", "在用+曾用");
		output_line(outfile, "th", "在用+曾用比例");
		if (state->backups_found == 1) {
			output_line(outfile, "th", "bu");
			output_line(outfile, "th", "bu perc");
		}
		end_tag(outfile, "tr");
		end_tag(outfile, "thead");
	}
	if (state->number_limit & S_BIT) {
		start_tag(outfile, "tbody");
		for (shared_p = state->shared_net_root->next; shared_p; shared_p = shared_p->next) {
			if (shnet_output_helper(state, &oh, shared_p))
				continue;
			start_tag(outfile, "tr");
			output_line(outfile, "td", shared_p->name);
			output_double(outfile, "td", shared_p->available);
			output_double(outfile, "td", shared_p->used);
			output_double(outfile, "td", shared_p->available - shared_p->used);
			output_double_color(state, &oh, outfile, "td");
			output_double(outfile, "td", shared_p->touched);
			output_double(outfile, "td", oh.tc);
			output_float(outfile, "td", oh.tcp);
			if (state->backups_found == 1) {
				output_double(outfile, "td", shared_p->backups);
				output_float(outfile, "td", oh.bup);
			}
			end_tag(outfile, "tr");
		}
		end_tag(outfile, "tbody");
	}
	table_end(outfile);*/
	newsection(outfile, "地址段");
	table_start(outfile, "r", "ranges");
	if (state->header_limit & R_BIT) {
		start_tag(outfile, "thead");
		start_tag(outfile, "tr");
		output_line(outfile, "th", "网段名称");
		output_line(outfile, "th", "起始IP");
		output_line(outfile, "th", "结束IP");
		output_line(outfile, "th", "总数");
		output_line(outfile, "th", "在用");
		output_line(outfile, "th", "空闲");
		output_line(outfile, "th", "比例");
		output_line(outfile, "th", "曾用");
		output_line(outfile, "th", "在用+曾用");
		output_line(outfile, "th", "在用+曾用比例");
		if (state->backups_found == 1) {
			output_line(outfile, "th", "bu");
			output_line(outfile, "th", "bu perc");
		}
		end_tag(outfile, "tr");
		end_tag(outfile, "thead");
	}
	if (state->number_limit & R_BIT) {
		start_tag(outfile, "tbody");
		for (i = 0; i < state->num_ranges; i++) {
			if (range_output_helper(state, &oh, range_p)) {
				range_p++;
				continue;
			}
			start_tag(outfile, "tr");
			if (range_p->shared_net) {
				output_line(outfile, "td", range_p->shared_net->name);
			} else {
				output_line(outfile, "td", "not_defined");
			}
			output_line(outfile, "td", ntop_ipaddr(&range_p->first_ip));
			output_line(outfile, "td", ntop_ipaddr(&range_p->last_ip));
			output_double(outfile, "td", oh.range_size);
			output_double(outfile, "td", range_p->count);
			output_double(outfile, "td", oh.range_size - range_p->count);
			output_double_color(state, &oh, outfile, "td");
			output_double(outfile, "td", range_p->touched);
			output_double(outfile, "td", oh.tc);
			output_float(outfile, "td", oh.tcp);
			if (state->backups_found == 1) {
				output_double(outfile, "td", range_p->backups);
				output_float(outfile, "td", oh.bup);
			}
			end_tag(outfile, "tr");
			range_p++;
		}
		end_tag(outfile, "tbody");
	}
	table_end(outfile);
	html_footer(outfile);
	close_outfile(outfile);
	return 0;
}

/*! \brief Output cvs format. */
static int output_csv(struct conf_t *state)
{
	unsigned int i;
	struct range_t *range_p;
	struct shared_network_t *shared_p;
	struct output_helper_t oh;
	FILE *outfile;

	outfile = open_outfile(state);
	range_p = state->ranges;
	if (state->header_limit & R_BIT) {
		fprintf(outfile, "\"Ranges:\"\n");
		fprintf
		    (outfile,
		     "\"shared net name\",\"first ip\",\"last ip\",\"max\",\"cur\",\"percent\",\"touch\",\"t+c\",\"t+c perc\"");
		if (state->backups_found == 1) {
			fprintf(outfile, ",\"bu\",\"bu perc\"");
		}
		fprintf(outfile, "\n");
	}
	if (state->number_limit & R_BIT) {
		for (i = 0; i < state->num_ranges; i++) {
			if (range_output_helper(state, &oh, range_p)) {
				range_p++;
				continue;
			}
			if (range_p->shared_net) {
				fprintf(outfile, "\"%s\",", range_p->shared_net->name);
			} else {
				fprintf(outfile, "\"not_defined\",");
			}
			fprintf(outfile, "\"%s\",", ntop_ipaddr(&range_p->first_ip));
			fprintf(outfile,
				"\"%s\",\"%g\",\"%g\",\"%.3f\",\"%g\",\"%g\",\"%.3f\"",
				ntop_ipaddr(&range_p->last_ip),
				oh.range_size,
				range_p->count,
				oh.percent,
				range_p->touched,
				oh.tc,
				oh.tcp);
			if (state->backups_found == 1) {
				fprintf(outfile, ",\"%g\",\"%.3f\"", range_p->backups, oh.bup);
			}

			fprintf(outfile, "\n");
			range_p++;
		}
		fprintf(outfile, "\n");
	}
	if (state->header_limit & S_BIT) {
		fprintf(outfile, "\"Shared networks:\"\n");
		fprintf(outfile,
			"\"name\",\"max\",\"cur\",\"percent\",\"touch\",\"t+c\",\"t+c perc\"");
		if (state->backups_found == 1) {
			fprintf(outfile, ",\"bu\",\"bu perc\"");
		}
		fprintf(outfile, "\n");
	}
	if (state->number_limit & S_BIT) {

		for (shared_p = state->shared_net_root->next; shared_p; shared_p = shared_p->next) {
			if (shnet_output_helper(state, &oh, shared_p))
				continue;
			fprintf(outfile,
				"\"%s\",\"%g\",\"%g\",\"%.3f\",\"%g\",\"%g\",\"%.3f\"",
				shared_p->name,
				shared_p->available,
				shared_p->used,
				oh.percent,
				shared_p->touched,
				oh.tc,
				oh.tcp);
			if (state->backups_found == 1) {
				fprintf(outfile, ",\"%g\",\"%.3f\"", shared_p->backups, oh.bup);
			}

			fprintf(outfile, "\n");
		}
		fprintf(outfile, "\n");
	}
	if (state->header_limit & A_BIT) {
		fprintf(outfile, "\"Sum of all ranges:\"\n");
		fprintf(outfile,
			"\"name\",\"max\",\"cur\",\"percent\",\"touch\",\"t+c\",\"t+c perc\"");
		if (state->backups_found == 1) {
			fprintf(outfile, ",\"bu\",\"bu perc\"");
		}
		fprintf(outfile, "\n");
	}
	if (state->number_limit & A_BIT) {
		shnet_output_helper(state, &oh, state->shared_net_root);
		fprintf(outfile,
			"\"%s\",\"%g\",\"%g\",\"%.3f\",\"%g\",\"%g\",\"%.3f\"",
			state->shared_net_root->name,
			state->shared_net_root->available,
			state->shared_net_root->used,
			oh.percent,
			state->shared_net_root->touched,
			oh.tc,
			oh.tcp);
		if (state->backups_found == 1) {
			fprintf(outfile, "%7g %8.3f", state->shared_net_root->backups, oh.bup);
		}
		fprintf(outfile, "\n");
	}
	close_outfile(outfile);
	return 0;
}

/*! \brief Output alarm text, and return program exit value. */
static int output_alarming(struct conf_t *state)
{
	FILE *outfile;
	struct range_t *range_p;
	struct shared_network_t *shared_p;
	struct output_helper_t oh;
	unsigned int i;
	int rw = 0, rc = 0, ro = 0, ri = 0, sw = 0, sc = 0, so = 0, si = 0;
	int ret_val;

	outfile = open_outfile(state);
	range_p = state->ranges;

	if (state->number_limit & R_BIT) {
		for (i = 0; i < state->num_ranges; i++) {
			range_output_helper(state, &oh, range_p);
			switch (oh.status) {
			case STATUS_SUPPRESSED:
				break;
			case STATUS_IGNORED:
				ri++;
				break;
			case STATUS_CRIT:
				rc++;
				break;
			case STATUS_WARN:
				rw++;
				break;
			case STATUS_OK:
				ro++;
				break;
			default:
				abort();
			}
			range_p++;
		}
	}
	if (state->number_limit & S_BIT) {
		for (shared_p = state->shared_net_root->next; shared_p; shared_p = shared_p->next) {
			shnet_output_helper(state, &oh, shared_p);
			switch (oh.status) {
			case STATUS_SUPPRESSED:
				break;
			case STATUS_IGNORED:
				si++;
				break;
			case STATUS_CRIT:
				sc++;
				break;
			case STATUS_WARN:
				sw++;
				break;
			case STATUS_OK:
				so++;
				break;
			default:
				abort();
			}
		}
	}

	if (sc || rc)
		ret_val = STATE_CRITICAL;
	else if (sw || rw)
		ret_val = STATE_WARNING;
	else
		ret_val = STATE_OK;

	if ((0 < rc && state->number_limit & R_BIT)
	    || (0 < sc && state->number_limit & S_BIT)) {
		fprintf(outfile, "CRITICAL: %s:", program_name);
	} else if ((0 < rw && state->number_limit & R_BIT)
		   || (0 < sw && state->number_limit & S_BIT)) {
		fprintf(outfile, "WARNING: %s:", program_name);
	} else {
		if (state->number_limit & A_BIT)
			fprintf(outfile, "OK:");
		else {
			if (close_stream(outfile)) {
				error(EXIT_FAILURE, errno, "output_alarming: fclose");
			}
			return ret_val;
		}
	}
	if (state->header_limit & R_BIT) {
		fprintf(outfile, " Ranges - crit: %d warn: %d ok: %d", rc, rw, ro);
		if (ri != 0) {
			fprintf(outfile, " ignored: %d", ri);
		}
		fprintf(outfile, "; | range_crit=%d range_warn=%d range_ok=%d", rc, rw, ro);
		if (ri != 0) {
			fprintf(outfile, " range_ignored=%d", ri);
		}
		if (state->perfdata == 1 && state->number_limit & R_BIT) {
			for (i = 0; i < state->num_ranges; i++) {
				range_p--;
				if (range_output_helper(state, &oh, range_p))
					continue;
				if (state->minsize < oh.range_size) {
					fprintf(outfile, " %s_r=", ntop_ipaddr(&range_p->first_ip));
					fprintf(outfile, "%g;%g;%g;0;%g",
						range_p->count,
						(oh.range_size * state->warning / 100),
						(oh.range_size * state->critical / 100), oh.range_size);
					fprintf(outfile, " %s_rt=%g",
						ntop_ipaddr(&range_p->first_ip), range_p->touched);
					if (state->backups_found == 1) {
						fprintf(outfile, " %s_rbu=%g",
							ntop_ipaddr(&range_p->first_ip),
							range_p->backups);
					}
				}
			}
		}
		fprintf(outfile, "\n");
	} else {
		fprintf(outfile, " ");
	}
	if (state->header_limit & S_BIT) {
		fprintf(outfile, "Shared nets - crit: %d warn: %d ok: %d", sc, sw, so);
		if (si != 0) {
			fprintf(outfile, " ignored: %d", si);
		}
		fprintf(outfile, "; | snet_crit=%d snet_warn=%d snet_ok=%d", sc, sw, so);
		if (si != 0) {
			fprintf(outfile, " snet_ignored=%d", si);
		}
		if (state->perfdata == 1 && state->header_limit & R_BIT) {
			for (shared_p = state->shared_net_root->next; shared_p; shared_p = shared_p->next) {
				if (shnet_output_helper(state, &oh, shared_p))
					continue;
				if (state->minsize < shared_p->available) {
					fprintf(outfile, " '%s_s'=%g;%g;%g;0;%g",
						shared_p->name,
						shared_p->used,
						(shared_p->available * state->warning / 100),
						(shared_p->available * state->critical / 100),
						shared_p->available);
					fprintf(outfile, " '%s_st'=%g",
						shared_p->name, shared_p->touched);
					if (state->backups_found == 1) {
						fprintf(outfile, " '%s_sbu'=%g",
							shared_p->name, shared_p->backups);
					}
				}
			}
			fprintf(outfile, "\n");
		}
	}
	fprintf(outfile, "\n");
	close_outfile(outfile);
	return ret_val;
}

/*! \brief Return output_format_names enum based on single char input. */
int output_analysis(struct conf_t *state, const char output_format)
{
	int ret = 1;

	switch (output_format) {
	case 't':
		state->output_format = OUT_FORM_TEXT;
		ret = output_txt(state);
		break;
	case 'a':
		ret = output_alarming(state);
		break;
	case 'h':
		error(EXIT_FAILURE, 0, "html table only output format is deprecated");
		break;
	case 'H':
		state->output_format = OUT_FORM_HTML;
		ret = output_html(state);
		break;
	case 'x':
		ret = output_xml(state, 0);
		break;
	case 'X':
		ret = output_xml(state, 1);
		break;
	case 'j':
		ret = output_json(state, 0);
		break;
	case 'J':
		ret = output_json(state, 1);
		break;
	case 'c':
		ret = output_csv(state);
		break;
#ifdef BUILD_MUSTACH
	case 'm':
		ret = mustach_dhcpd_pools(state);
		break;
#endif
	default:
		error(EXIT_FAILURE, 0, "unknown output format: '%c'", output_format);
	}
	return ret;
}
