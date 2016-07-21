/**
 * This header is intended to fix inline problem with
 * Boost.Log macros BOOST_LOG, BOOST_LOG_SEV and others.
 * In MSVC __forceinline will fail if function returns
 * unwindable object by value. This is happening in
 * function boost::log::aux::make_record_pump which is
 * declared in boost/log/sources/record_ostream.hpp header.
 */

#ifndef FIX_BOOST_LOG_SOURCES_RECORD_OSTREAM_HPP_INCLUDED_
#define FIX_BOOST_LOG_SOURCES_RECORD_OSTREAM_HPP_INCLUDED_

#include <boost/typeof/typeof.hpp>

#include BOOST_TYPEOF_INCREMENT_REGISTRATION_GROUP()

#if !defined(BOOST_LOG_STREAM_WITH_PARAMS_INTERNAL) || !defined(BOOST_LOG_STREAM_INTERNAL)
#  error "Is Boost.Log included?"
#endif

#undef BOOST_LOG_STREAM_WITH_PARAMS_INTERNAL
#undef BOOST_LOG_STREAM_INTERNAL

BOOST_TYPEOF_REGISTER_TEMPLATE(::boost::log::aux::record_pump, 1)

#define BOOST_LOG_STREAM_INTERNAL(logger, rec_var)\
    for (::boost::log::record rec_var = (logger).open_record(); !!rec_var;)\
        ::boost::log::aux::record_pump<BOOST_TYPEOF(logger)>((logger), rec_var).stream()

#define BOOST_LOG_STREAM_WITH_PARAMS_INTERNAL(logger, rec_var, params_seq)\
    for (::boost::log::record rec_var = (logger).open_record((BOOST_PP_SEQ_ENUM(params_seq))); !!rec_var;)\
        ::boost::log::aux::record_pump<BOOST_TYPEOF(logger)>((logger), rec_var).stream()

#endif
