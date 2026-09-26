# Copyright 2026 Qore Technologies, s.r.o.
#
# Fixes a c-ares 1.34.7/1.34.8 defect that loses a DNS query, so that the lookup never completes, and the
# use-after-free the fix alone would expose (c-ares issue #1280, pull request #1256 and its review).
#
# ares_flush_requeue() detaches a completed query (removing its qid from the channel's queries_by_qid
# table) before invoking its callback, and ares_free_query() detaches it a second time afterwards.  When
# the callback starts a new query - ares_getaddrinfo() moving on to the next search domain - and that
# query is given the qid just freed (about 1 in 65536), the second detach removes the new query's
# mapping: its answer is then dropped as unknown and its retry is discarded, and the lookup never ends.
# 1. ares_detach_query() only removes the mapping if it still refers to the query being detached.
# 2. ares_send_query() decides whether the query it sent still exists by looking up its qid; with the
#    first fix a reused qid can then find the new query, report success for a freed query, and make
#    ares_send_nolock() write the qid into freed memory.  It compares the query instead: its address is
#    only compared, never dereferenced.
#
# Invoked from FetchContent_Declare's PATCH_COMMAND, where the working directory is the populated
# source directory.  The patch is idempotent.  If neither the original nor the patched text of a change is
# found, the source changed: fail, so that updating c-ares checks whether this patch is still needed.
set(_file "src/lib/ares_process.c")
file(READ "${_file}" _content)

set(_orig_1 "  ares_query_remove_from_conn(query);
  ares_htable_szvp_remove(query->channel->queries_by_qid, query->qid);
  ares_llist_node_destroy(query->node_all_queries);")
set(_fixed_1 "  ares_query_remove_from_conn(query);
  /* The query may already have been detached (ares_flush_requeue() detaches it before invoking its
   * callback), and the callback may have started a new query with the freed qid: only remove the qid
   * mapping if it still refers to this query */
  if (ares_htable_szvp_get_direct(query->channel->queries_by_qid, query->qid) == query) {
    ares_htable_szvp_remove(query->channel->queries_by_qid, query->qid);
  }
  ares_llist_node_destroy(query->node_all_queries);")

set(_orig_2 "  if (status == ARES_SUCCESS &&
      ares_htable_szvp_get_direct(channel->queries_by_qid, qid) == NULL) {
    status = ARES_ETIMEOUT;
  }")
set(_fixed_2 "  /* A new query sent from a callback can have been given the same qid, so the query itself is compared;
   * its address is only compared, never dereferenced */
  if (status == ARES_SUCCESS &&
      ares_htable_szvp_get_direct(channel->queries_by_qid, qid) != query) {
    status = ARES_ETIMEOUT;
  }")

foreach(_n 1 2)
    string(FIND "${_content}" "${_fixed_${_n}}" _fixed_pos)
    if (_fixed_pos GREATER -1)
        continue()
    endif()
    string(FIND "${_content}" "${_orig_${_n}}" _orig_pos)
    if (_orig_pos EQUAL -1)
        message(FATAL_ERROR "c-ares ${_file}: change ${_n} of the lost-query fix does not apply; check whether "
            "cmake/PatchCaresLostQuery.cmake is still needed")
    endif()
    string(REPLACE "${_orig_${_n}}" "${_fixed_${_n}}" _content "${_content}")
endforeach()
file(WRITE "${_file}" "${_content}")
