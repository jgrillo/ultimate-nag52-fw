
/**
* AUTOGENERATED BY convert.py
* DO NOT EDIT THIS FILE!
*
* IF MODIFICATIONS NEED TO BE MADE, MODIFY can_data.txt!
*
* CAN Defintiion for ECU 'MPC'
*/

#ifdef EGS_SPRINTER_MODE

#ifndef __ECU_MPC_H_
#define __ECU_MPC_H_

#include <stdint.h>
    
#define MPC_IHC_CAN_ID 0x018B
#define ALDW_A1_CAN_ID 0x021F

/** IHC State MPC */
enum class MPC_IHC_IHC_Stat_MPC {
	IHC_ACTV = 0, // unknown
	IHC_FLT = 1, // unknown
	TEMP_NAVL = 2, // unknown
	SNA = 3, // unknown
};

/** IHC Weather Detection */
enum class MPC_IHC_IHC_Wthr_Dtct {
	NO_DTCT = 0, // unknown
	ADV_WTHR_DTCT = 1, // unknown
	NDEF = 2, // unknown
	SNA = 3, // unknown
};

/** IHC City Detection */
enum class MPC_IHC_IHC_City_Dtct {
	NO_DTCT = 0, // unknown
	CITY_DTCT = 1, // unknown
	NDEF = 2, // unknown
	SNA = 3, // unknown
};

/** IHC High Beam State */
enum class MPC_IHC_IHC_HiBm_Stat {
	OFF = 0, // unknown
	ON = 1, // unknown
	NDEF = 2, // unknown
	SNA = 3, // unknown
};

/** Aldw Indication Lamp or Switch LED on Request */
enum class ALDW_A1_ALDW_IndLmp_SwLED_On_Rq {
	OFF = 0, // unknown
	ON = 1, // unknown
	READY = 2, // unknown
	SNA = 3, // unknown
};

/** ALDW Message Display Request */
enum class ALDW_A1_ALDW_MsgDisp_Rq {
	IDLE = 0, // unknown
	ALDW_TEMP_NA = 1, // unknown
	ALDW_NA = 2, // unknown
	SNA = 7, // unknown
};

/** ALDW WARNING REQUEST */
enum class ALDW_A1_ALDW_Warn_Rq {
	IDLE = 0, // unknown
	WARN_LT = 1, // unknown
	WARN_RT = 2, // unknown
	SNA = 7, // unknown
};



typedef union {
	uint64_t raw;
	uint8_t bytes[8];

	/** Gets CAN ID of MPC_IHC */
	uint32_t get_canid(){ return MPC_IHC_CAN_ID; }
    /** Sets IHC State MPC */
    void set_IHC_Stat_MPC(MPC_IHC_IHC_Stat_MPC value){ raw = (raw & 0xfcffffffffffffff) | ((uint64_t)value & 0x3) << 56; }

    /** Gets IHC State MPC */
    MPC_IHC_IHC_Stat_MPC get_IHC_Stat_MPC() const { return (MPC_IHC_IHC_Stat_MPC)(raw >> 56 & 0x3); }
        
    /** Sets IHC Weather Detection */
    void set_IHC_Wthr_Dtct(MPC_IHC_IHC_Wthr_Dtct value){ raw = (raw & 0xffff3fffffffffff) | ((uint64_t)value & 0x3) << 46; }

    /** Gets IHC Weather Detection */
    MPC_IHC_IHC_Wthr_Dtct get_IHC_Wthr_Dtct() const { return (MPC_IHC_IHC_Wthr_Dtct)(raw >> 46 & 0x3); }
        
    /** Sets IHC City Detection */
    void set_IHC_City_Dtct(MPC_IHC_IHC_City_Dtct value){ raw = (raw & 0xffffcfffffffffff) | ((uint64_t)value & 0x3) << 44; }

    /** Gets IHC City Detection */
    MPC_IHC_IHC_City_Dtct get_IHC_City_Dtct() const { return (MPC_IHC_IHC_City_Dtct)(raw >> 44 & 0x3); }
        
    /** Sets IHC High Beam State */
    void set_IHC_HiBm_Stat(MPC_IHC_IHC_HiBm_Stat value){ raw = (raw & 0xfffff3ffffffffff) | ((uint64_t)value & 0x3) << 42; }

    /** Gets IHC High Beam State */
    MPC_IHC_IHC_HiBm_Stat get_IHC_HiBm_Stat() const { return (MPC_IHC_IHC_HiBm_Stat)(raw >> 42 & 0x3); }
        
    /** Sets Message Counter. Conversion formula (To raw from real): y=(x-0.0)/1.00 */
    void set_MC_MPC_IHC(uint8_t value){ raw = (raw & 0xffffffffffff0fff) | ((uint64_t)value & 0xf) << 12; }

    /** Gets Message Counter. Conversion formula (To real from raw): y=(1.00x)+0.0 */
    uint8_t get_MC_MPC_IHC() const { return (uint8_t)(raw >> 12 & 0xf); }
        
} MPC_IHC;



typedef union {
	uint64_t raw;
	uint8_t bytes[8];

	/** Gets CAN ID of ALDW_A1 */
	uint32_t get_canid(){ return ALDW_A1_CAN_ID; }
    /** Sets Aldw Indication Lamp or Switch LED on Request */
    void set_ALDW_IndLmp_SwLED_On_Rq(ALDW_A1_ALDW_IndLmp_SwLED_On_Rq value){ raw = (raw & 0xcfffffffffffffff) | ((uint64_t)value & 0x3) << 60; }

    /** Gets Aldw Indication Lamp or Switch LED on Request */
    ALDW_A1_ALDW_IndLmp_SwLED_On_Rq get_ALDW_IndLmp_SwLED_On_Rq() const { return (ALDW_A1_ALDW_IndLmp_SwLED_On_Rq)(raw >> 60 & 0x3); }
        
    /** Sets ALDW MENU OR SWITCH ENABLE REQUEST */
    void set_ALDW_Menu_Sw_Enbl_Rq(bool value){ raw = (raw & 0xf7ffffffffffffff) | ((uint64_t)value & 0x1) << 59; }

    /** Gets ALDW MENU OR SWITCH ENABLE REQUEST */
    bool get_ALDW_Menu_Sw_Enbl_Rq() const { return (bool)(raw >> 59 & 0x1); }
        
    /** Sets ALDW Message Display Request */
    void set_ALDW_MsgDisp_Rq(ALDW_A1_ALDW_MsgDisp_Rq value){ raw = (raw & 0xf8ffffffffffffff) | ((uint64_t)value & 0x7) << 56; }

    /** Gets ALDW Message Display Request */
    ALDW_A1_ALDW_MsgDisp_Rq get_ALDW_MsgDisp_Rq() const { return (ALDW_A1_ALDW_MsgDisp_Rq)(raw >> 56 & 0x7); }
        
    /** Sets ALDW WARNING REQUEST */
    void set_ALDW_Warn_Rq(ALDW_A1_ALDW_Warn_Rq value){ raw = (raw & 0xfff8ffffffffffff) | ((uint64_t)value & 0x7) << 48; }

    /** Gets ALDW WARNING REQUEST */
    ALDW_A1_ALDW_Warn_Rq get_ALDW_Warn_Rq() const { return (ALDW_A1_ALDW_Warn_Rq)(raw >> 48 & 0x7); }
        
    /** Sets Message Counter. Conversion formula (To raw from real): y=(x-0.0)/1.00 */
    void set_MC_ALDW_A1(uint8_t value){ raw = (raw & 0xffffffffffff0fff) | ((uint64_t)value & 0xf) << 12; }

    /** Gets Message Counter. Conversion formula (To real from raw): y=(1.00x)+0.0 */
    uint8_t get_MC_ALDW_A1() const { return (uint8_t)(raw >> 12 & 0xf); }
        
    /** Sets CRC Checksum Byte 1 to 7 Accordinging to SAE J1850. Conversion formula (To raw from real): y=(x-0.0)/1.00 */
    void set_CRC_ALDW_A1(uint8_t value){ raw = (raw & 0xffffffffffffff00) | ((uint64_t)value & 0xff) << 0; }

    /** Gets CRC Checksum Byte 1 to 7 Accordinging to SAE J1850. Conversion formula (To real from raw): y=(1.00x)+0.0 */
    uint8_t get_CRC_ALDW_A1() const { return (uint8_t)(raw >> 0 & 0xff); }
        
} ALDW_A1;



class ECU_MPC {
	public:
        /**
         * @brief Imports the CAN frame given the CAN ID, CAN Contents, and current timestamp
         *
         * Returns true if the frame was imported successfully, and false if import failed (Due to non-matching CAN ID).
         *
         * NOTE: The endianness of the value cannot be guaranteed. It is up to the caller to correct the byte order!
         */
        bool import_frames(uint64_t value, uint32_t can_id, uint64_t timestamp_now) {
            switch(can_id) {
                case MPC_IHC_CAN_ID:
                    LAST_FRAME_TIMES[0] = timestamp_now;
                    FRAME_DATA[0] = value;
                    return true;
                case ALDW_A1_CAN_ID:
                    LAST_FRAME_TIMES[1] = timestamp_now;
                    FRAME_DATA[1] = value;
                    return true;
                default:
                    return false;
            }
        }
        
        /** Sets data in pointer to MPC_IHC
          * 
          * If this function returns false, then the CAN Frame is invalid or has not been seen
          * on the CANBUS network yet. Meaning it's data cannot be used.
          *
          * If the function returns true, then the pointer to 'dest' has been updated with the new CAN data
          */
        bool get_MPC_IHC(uint64_t now, uint64_t max_expire_time, MPC_IHC* dest) const {
            if (LAST_FRAME_TIMES[0] == 0 || dest == nullptr) { // CAN Frame has not been seen on bus yet / NULL pointer
                return false;
            } else if (now - LAST_FRAME_TIMES[0] > max_expire_time) { // CAN Frame has not refreshed in valid interval
                return false;
            } else { // CAN Frame is valid! return it
                dest->raw = FRAME_DATA[0];
                return true;
            }
        }
            
        /** Sets data in pointer to ALDW_A1
          * 
          * If this function returns false, then the CAN Frame is invalid or has not been seen
          * on the CANBUS network yet. Meaning it's data cannot be used.
          *
          * If the function returns true, then the pointer to 'dest' has been updated with the new CAN data
          */
        bool get_ALDW_A1(uint64_t now, uint64_t max_expire_time, ALDW_A1* dest) const {
            if (LAST_FRAME_TIMES[1] == 0 || dest == nullptr) { // CAN Frame has not been seen on bus yet / NULL pointer
                return false;
            } else if (now - LAST_FRAME_TIMES[1] > max_expire_time) { // CAN Frame has not refreshed in valid interval
                return false;
            } else { // CAN Frame is valid! return it
                dest->raw = FRAME_DATA[1];
                return true;
            }
        }
            
	private:
		uint64_t FRAME_DATA[2];
		uint64_t LAST_FRAME_TIMES[2];
};
#endif // __ECU_MPC_H_

#endif // EGS_SPRINTER_MODE