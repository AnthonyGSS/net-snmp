/*
 * Note: this file originally auto-generated by mib2c using
 *  : mib2c.table_data.conf,v 1.3 2004/10/14 12:57:34 dts12 Exp $
 */

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include "disman/schedCore.h"
#include "utilities/iquery.h"

netsnmp_table_data *schedTable;

/** Initializes the schedCore module */
void
init_schedCore(void)
{
    netsnmp_table_row *row;
    struct schedTable_entry *entry;

    /*
     * Create a table structure for the schedule table
     * This will typically be registered by the schedTable module
     */
    DEBUGMSGTL(("sched", "Initializing core module\n"));
    schedTable = netsnmp_create_table_data("schedTable");

#ifdef NO_TESTING
    /*
     * Insert an entry into this table for testing 
     */
    row = schedTable_createEntry(schedTable, "dts", 3, "test2", 5, NULL );
    entry = (struct schedTable_entry *)row->data;
    init_snmp("snmpd");       /* HACK - to load the MIB files */
    entry->schedVariable_len = MAX_OID_LEN;
    if (snmp_parse_oid("UCD-SNMP-MIB::versionDoDebugging.0", entry->schedVariable,
                                              &entry->schedVariable_len)) {
        entry->schedAdminStatus = 1;  /* Enable the entry */
        entry->schedRowStatus   = 1;
        entry->schedInterval    = 20;
        sched_nextTime( entry );
    } else {
        snmp_perror("versionDoDebugging.0");
    }
#endif
}


/*
 * Callback to invoke a scheduled action
 */
static void
_sched_callback( unsigned int reg, void *magic )
{
    struct schedTable_entry *entry = (struct schedTable_entry *)magic;
    int ret;
    netsnmp_variable_list assign;
    memset(&assign, 0, sizeof(netsnmp_variable_list));

    if ( !entry ) {
        DEBUGMSGTL(("sched", "callback: no entry\n"));
        return;
    }
    entry->schedLastRun = time(0);
    entry->schedTriggers++;

    DEBUGMSGTL(( "sched", "callback: "));
    DEBUGMSGOID(("sched", entry->schedVariable, entry->schedVariable_len));
    DEBUGMSG((   "sched", " = %d\n", entry->schedValue));
    snmp_set_var_objid(&assign, entry->schedVariable, entry->schedVariable_len);
    snmp_set_var_typed_value(&assign, ASN_INTEGER,
                                         (u_char *)&entry->schedValue,
                                         sizeof(entry->schedValue));

    ret = netsnmp_query_set( &assign, entry->schedSession );
    if ( ret != SNMP_ERR_NOERROR ) {
        entry->schedFailures++;
        entry->schedLastFailure = ret;
        time ( &entry->schedLastFailed );
    }

    sched_nextTime( entry );
}

    /*
     * Internal utility routines to help interpret
     *  calendar-based schedule bit strings
     */
static char _masks[] = { /* 0xff, */ 0x7f, 0x3f, 0x1f,
                         0x0f, 0x07, 0x03, 0x01, 0x00 };
static char _bits[]  = { 0x80, 0x40, 0x20, 0x10,
                         0x08, 0x04, 0x02, 0x01 };

/*
 * Are any of the bits set?
 */
static int
_bit_allClear( char *pattern, int len ) {
    int i;

    for (i=0; i<len; i++) {
        if ( pattern[i] != 0 )
            return 0;    /* At least one bit set */
    }
    return 1;  /* All bits clear */ 
}

/*
 * Is a particular bit set?
 */
static int
_bit_set( char *pattern, int bit ) {
    int major, minor;
    char buf[ 8 ];
    memset( buf, 0, 8 );
    memcpy( buf, pattern, 4 );

    major = bit/8;
    minor = bit%8;
    if ( buf[major] & _bits[minor] ) {
        return 1; /* Specified bit is set */
    }
    return 0;     /* Bit not set */
}

/*
 * What is the next bit set?
 *   (after a specified point)
 */
static int
_bit_next( char *pattern, int current, size_t len ) {
    char buf[ 8 ];
    int major, minor, i, j;

        /* Make a working copy of the bit pattern */
    memset( buf, 0, 8 );
    memcpy( buf, pattern, len );

        /*
         * If we're looking for the first bit after some point,
         * then clear all earlier bits from the working copy.
         */
    if ( current > -1 ) {
        major = current/8;
        minor = current%8;
        for ( i=0; i<major; i++ )
            buf[i]=0;
        buf[major] &= _masks[minor];
    }

        /*
         * Look for the first bit that's set
         */
    for ( i=0; i<len; i++ ) {
        if ( buf[i] != 0 ) {
            major = i*8;
            for ( j=0; j<8; j++ ) {
                if ( buf[i] & _bits[j] ) {
                    return major+j;
                }
            }
        }
    }
    return -1;     /* No next bit */
}


static int _daysPerMonth[] = { 31, 28, 31, 30,
                               31, 30, 31, 31,
                               30, 31, 30, 31, 29 };

static char _truncate[] = { 0xfe, 0xf0, 0xfe, 0xfc,
                            0xfe, 0xfc, 0xfe, 0xfe,
                            0xfc, 0xfe, 0xfc, 0xfe, 0xf8 };

/*
 * What is the next day with a relevant bit set?
 *
 * Merge the forward and reverse day bits into a single
 *   pattern relevant for this particular month,
 *   and apply the standard _bit_next() call.
 * Then check this result against the day of the week bits.
 */
static int
_bit_next_day( char *day_pattern, char weekday_pattern, int day, int month ) {
    char buf[4];
    int next_day, i;

        /* Make a working copy of the forward day bits ... */
    memset( buf,  0, 4 );
    memcpy( buf,  day_pattern, 4 );

        /* XXX - TODO: Handle reverse-day bits */
    buf[3] &= _truncate[ month ];

    next_day = day-1;  /* tm_day is 1-based, not 0-based */
    do {
        next_day = _bit_next( buf, next_day, 4 );
        if ( next_day < 0 )
            return -1;

    } while ( 0 /* XXX - Check this against the weekday mask */ );
    return next_day+1; /* Convert back to 1-based list */
}


/*
 * determine the time for the next scheduled action of a given entry
 */
void
sched_nextTime( struct schedTable_entry *entry )
{
    time_t now;
    struct tm now_tm, next_tm;
    int rev_day, mon;

    time( &now );

    if ( !entry ) {
        DEBUGMSGTL(("sched", "nextTime: no entry\n"));
        return;
    }

    if ( entry->schedCallbackID )
        snmp_alarm_unregister( entry->schedCallbackID );

    if ( entry->schedAdminStatus != 1  /* enabled */ ||
         entry->schedRowStatus   != 1  /* active  */ ) {
        DEBUGMSGTL(("sched", "nextTime: not active\n"));
        return;
    }

    switch ( entry->schedType ) {
    case 1:     /* periodic */
        if ( !entry->schedInterval ) {
            DEBUGMSGTL(("sched", "nextTime: no interval\n"));
            return;
        }
        if ( entry->schedLastRun ) {
             entry->schedNextRun = entry->schedLastRun +
                                   entry->schedInterval;
        } else {
             entry->schedNextRun = now + entry->schedInterval;
        }
        DEBUGMSGTL(("sched", "nextTime: periodic (%d) %s",
                                  entry->schedNextRun,
                           ctime(&entry->schedNextRun)));
        break;

    case 3:     /* one-shot */
        if ( entry->schedLastRun ) {
            DEBUGMSGTL(("sched", "nextTime: one-shot expired: (%d) %s",
                                  entry->schedLastRun,
                           ctime(&entry->schedLastRun)));
            return;
        }
        /* Fallthrough */
        DEBUGMSGTL(("sched", "nextTime: one-shot fallthrough\n"));
    case 2:     /* calendar */
        /*
         *  Check for complete time specification
         *  If any of the five fields have no bits set,
         *    the entry can't possibly match any time.
         */
        if ( _bit_allClear( entry->schedMinute, 8 ) ||
             _bit_allClear( entry->schedHour,   3 ) ||
             _bit_allClear( entry->schedDay,  4+4 ) ||
             _bit_allClear( entry->schedMonth,  2 ) ||
             _bit_allClear(&entry->schedWeekDay, 1 )) {
            DEBUGMSGTL(("sched", "nextTime: incomplete calendar spec\n"));
            return;
        }

        /*
         *  Calculate the next run time:
         *
         *  If the current Month, Day & Hour bits are set
         *    calculate the next specified minute
         *  If this fails (or the current Hour bit is not set)
         *    use the first specified minute,
         *    and calculate the next specified hour
         *  If this fails (or the current Day bit is not set)
         *    use the first specified minute and hour
         *    and calculate the next specified day (in this month)
         *  If this fails (or the current Month bit is not set)
         *    use the first specified minute and hour
         *    calculate the next specified month, and
         *    the first specified day (in that month)
         */

        localtime_r( &now, &now_tm );
        localtime_r( &now, &next_tm );

        next_tm.tm_mon=-1;
        next_tm.tm_mday=-1;
        next_tm.tm_hour=-1;
        next_tm.tm_min=-1;
        next_tm.tm_sec=0;
        if ( _bit_set( entry->schedMonth, now_tm.tm_mon )) {
            next_tm.tm_mon = now_tm.tm_mon;
            rev_day = _daysPerMonth[ now_tm.tm_mon ] - now_tm.tm_mday;
                /* XXX - TODO: Check reverse and week day bits */
            if ( _bit_set( entry->schedDay, now_tm.tm_mday-1 )) {
                next_tm.tm_mday = now_tm.tm_mday;

                if ( _bit_set( entry->schedHour, now_tm.tm_hour )) {
                    next_tm.tm_hour = now_tm.tm_hour;
                    /* XXX - Check Fall timechange */
                    next_tm.tm_min = _bit_next( entry->schedMinute,
                                                  now_tm.tm_min, 8 );
                } else {
                    next_tm.tm_min = -1;
                }
   
                if ( next_tm.tm_min == -1 ) {
                    next_tm.tm_min  = _bit_next( entry->schedMinute, -1, 8 );
                    next_tm.tm_hour = _bit_next( entry->schedHour,
                                                   now_tm.tm_hour, 3 );
                }
            } else {
                next_tm.tm_hour = -1;
            }

            if ( next_tm.tm_hour == -1 ) {
                next_tm.tm_min  = _bit_next( entry->schedMinute, -1, 8 );
                next_tm.tm_hour = _bit_next( entry->schedHour,   -1, 3 );
                    /* Handle leap years */
                mon = now_tm.tm_mon;
                if ( mon == 1 && (now_tm.tm_year%4 == 0) )
                    mon = 12;
                next_tm.tm_mday = _bit_next_day( entry->schedDay,
                                                 entry->schedWeekDay,
                                                 now_tm.tm_mday, mon );
            }
        } else {
            next_tm.tm_min  = _bit_next( entry->schedMinute, -1, 2 );
            next_tm.tm_hour = _bit_next( entry->schedHour,   -1, 3 );
            next_tm.tm_mday = -1;
            next_tm.tm_mon  = now_tm.tm_mon;
        }

        while ( next_tm.tm_mday == -1 ) {
            next_tm.tm_mon  = _bit_next( entry->schedMonth,
                                          next_tm.tm_mon, 2 );
            if ( next_tm.tm_mon == -1 ) {
                next_tm.tm_year++;
                next_tm.tm_mon  = _bit_next( entry->schedMonth,
                                             -1, 2 );
            }
                /* Handle leap years */
            mon = next_tm.tm_mon;
            if ( mon == 1 && (next_tm.tm_year%4 == 0) )
                mon = 12;
            next_tm.tm_mday = _bit_next_day( entry->schedDay,
                                             entry->schedWeekDay,
                                             -1, mon );
            /* XXX - catch infinite loop */
        }

        /* XXX - Check for Spring timechange */

        /*
         * 'next_tm' now contains the time for the next scheduled run
         */
        entry->schedNextRun = mktime( &next_tm );
        DEBUGMSGTL(("sched", "nextTime: calendar (%d) %s",
                                  entry->schedNextRun,
                           ctime(&entry->schedNextRun)));
        return;

    default:
        DEBUGMSGTL(("sched", "nextTime: unknown type %d\n", entry->schedType));
        return;
    }
    entry->schedCallbackID = snmp_alarm_register(
                                entry->schedNextRun - now,
                                0, _sched_callback, entry );
    return;
}

void
sched_nextRowTime( netsnmp_table_row *row )
{
    sched_nextTime((struct schedTable_entry *) row->data );
}

/*
 * create a new row in the table 
 */
netsnmp_table_row *
schedTable_createEntry_auth(netsnmp_table_data *table_data,
                       char *schedOwner, size_t schedOwner_len,
                       char *schedName,  size_t schedName_len,
                       int   version,    char  *secName,
                       int   secModel,   int    secLevel,
                       u_char *engineID, size_t engineID_len)
{
    struct schedTable_entry *entry;
    netsnmp_table_row *row;

    DEBUGMSGTL(("sched", "creating entry (%s/%s)\n", schedOwner, schedName));
    entry = SNMP_MALLOC_TYPEDEF(struct schedTable_entry);
    if (!entry)
        return NULL;

    row = netsnmp_create_table_data_row();
    if (!row) {
        SNMP_FREE(entry);
        return NULL;
    }
    row->data = entry;
    /*
     * Set the indexing for this entry, both in the row
     *  data structure, and in the table_data helper.
     */
    memcpy(entry->schedOwner, schedOwner, schedOwner_len);
    entry->schedOwner_len = schedOwner_len;
    netsnmp_table_row_add_index(row, ASN_OCTET_STR,
                                entry->schedOwner, schedOwner_len);
    memcpy(entry->schedName, schedName, schedName_len);
    entry->schedName_len = schedName_len;
    netsnmp_table_row_add_index(row, ASN_OCTET_STR,
                                entry->schedName, schedName_len);
    /*
     * Set the (non-zero) default values in the row data structure.
     */
    entry->schedType         = 1;   /* periodic */
    entry->schedAdminStatus  = 2;   /* disabled */
    entry->schedStorageType  = 2;   /* volatile */
    entry->schedVariable_len = 2;   /* .0.0 */

    entry->schedSession = netsnmp_iquery_session(secName,
                                                 version, secModel, secLevel,
                                                 engineID, engineID_len);

    netsnmp_table_data_add_row(table_data, row);
    return row;
}

netsnmp_table_row *
schedTable_createEntry_pdu(netsnmp_table_data *table_data,
                       char *schedOwner, size_t schedOwner_len,
                       char *schedName,  size_t schedName_len,
                       netsnmp_pdu *pdu)
{
    char *secName;

    if (!pdu)
        return NULL;
    if ( pdu->version == SNMP_VERSION_1 ||
         pdu->version == SNMP_VERSION_2c )
        secName = pdu->community;
    else
        secName = pdu->securityName;

    return schedTable_createEntry_auth(table_data,
                       schedOwner, schedOwner_len,
                       schedName,  schedName_len,
                       pdu->version,          secName,
                       pdu->securityModel,    pdu->securityLevel,
                       pdu->securityEngineID, pdu->securityEngineIDLen);
}

netsnmp_table_row *
schedTable_createEntry(netsnmp_table_data *table_data,
                       char *schedOwner, size_t schedOwner_len,
                       char *schedName,  size_t schedName_len)
{
    char *secName  = netsnmp_ds_get_string(NETSNMP_DS_APPLICATION_ID,
                                           NETSNMP_DS_AGENT_INTERNAL_SECNAME);
    int   version  = netsnmp_ds_get_int(   NETSNMP_DS_APPLICATION_ID,
                                           NETSNMP_DS_AGENT_INTERNAL_VERSION);
    int   secLevel = netsnmp_ds_get_int(   NETSNMP_DS_APPLICATION_ID,
                                           NETSNMP_DS_AGENT_INTERNAL_SECLEVEL);
    int   secModel;
    u_char eID[SNMP_MAXBUF_SMALL];
    size_t elen = snmpv3_get_engineID(eID, sizeof(eID));

    if (version == DEFAULT_SNMP_VERSION)
        version =  SNMP_VERSION_3;

    if (version == SNMP_VERSION_3) {
        secModel = SNMP_SEC_MODEL_USM;
        if (!secLevel)
            secLevel = SNMP_SEC_LEVEL_AUTHNOPRIV;
    } else {
        secModel = version+1;
        secLevel = SNMP_SEC_LEVEL_NOAUTH;
    }
    
    return schedTable_createEntry_auth(table_data,
                       schedOwner, schedOwner_len,
                       schedName,  schedName_len,
                       version,    secName,
                       secModel,   secLevel,
                       eID,        elen);
}

/*
 * remove a row from the table 
 */
void
schedTable_removeEntry(netsnmp_table_data *table_data,
                       netsnmp_table_row *row)
{
    struct schedTable_entry *entry;

    DEBUGMSGTL(("sched", "removing entry (%x)\n", row));
    if (!row)
        return;                 /* Nothing to remove */
    entry = (struct schedTable_entry *)
        netsnmp_table_data_remove_and_delete_row(table_data, row);
    if (entry)
        SNMP_FREE(entry);       /* XXX - release any other internal resources */
}
