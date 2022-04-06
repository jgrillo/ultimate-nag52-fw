
/**
* AUTOGENERATED BY convert.py
* DO NOT EDIT THIS FILE!
*
* IF MODIFICATIONS NEED TO BE MADE, MODIFY can_data.txt!
*
* CAN Defintiion for ECU 'BSM_S'
*/

#ifdef EGS_SPRINTER_MODE

#ifndef __ECU_BSM_S_H_
#define __ECU_BSM_S_H_

#include <stdint.h>
    
#define BSM_SLAVE_RQ_CAN_ID 0x0182

/** Status BSM Slave */
enum class BSM_SLAVE_RQ_BSM_SlaveAvl_State {
	OK = 0, // unknown
	BSM_PERM = 1, // unknown
	BSM_TEMP = 2, // unknown
	SNA = 7, // unknown
};

/** object handover */
enum class BSM_SLAVE_RQ_BSM_Slave_Obj_Trans {
	NOOBJ = 0, // unknown
	OBJ1 = 1, // unknown
	OBJ2 = 2, // unknown
};

/** BSM slave requirement */
enum class BSM_SLAVE_RQ_BSM_SlaveWarn_Rq {
	AUS = 0, // unknown
	EIN = 1, // unknown
	NDEF2 = 2, // unknown
	SNA = 3, // unknown
};



typedef union {
	uint64_t raw;
	uint8_t bytes[8];

	/** Gets CAN ID of BSM_SLAVE_RQ */
	uint32_t get_canid(){ return BSM_SLAVE_RQ_CAN_ID; }
    /** Sets Status BSM Slave */
    void set_BSM_SlaveAvl_State(BSM_SLAVE_RQ_BSM_SlaveAvl_State value){ raw = (raw & 0x8fffffffffffffff) | ((uint64_t)value & 0x7) << 60; }

    /** Gets Status BSM Slave */
    BSM_SLAVE_RQ_BSM_SlaveAvl_State get_BSM_SlaveAvl_State() const { return (BSM_SLAVE_RQ_BSM_SlaveAvl_State)(raw >> 60 & 0x7); }
        
    /** Sets object handover */
    void set_BSM_Slave_Obj_Trans(BSM_SLAVE_RQ_BSM_Slave_Obj_Trans value){ raw = (raw & 0xf3ffffffffffffff) | ((uint64_t)value & 0x3) << 58; }

    /** Gets object handover */
    BSM_SLAVE_RQ_BSM_Slave_Obj_Trans get_BSM_Slave_Obj_Trans() const { return (BSM_SLAVE_RQ_BSM_Slave_Obj_Trans)(raw >> 58 & 0x3); }
        
    /** Sets BSM slave requirement */
    void set_BSM_SlaveWarn_Rq(BSM_SLAVE_RQ_BSM_SlaveWarn_Rq value){ raw = (raw & 0xfcffffffffffffff) | ((uint64_t)value & 0x3) << 56; }

    /** Gets BSM slave requirement */
    BSM_SLAVE_RQ_BSM_SlaveWarn_Rq get_BSM_SlaveWarn_Rq() const { return (BSM_SLAVE_RQ_BSM_SlaveWarn_Rq)(raw >> 56 & 0x3); }
        
} BSM_SLAVE_RQ;



class ECU_BSM_S {
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
                case BSM_SLAVE_RQ_CAN_ID:
                    LAST_FRAME_TIMES[0] = timestamp_now;
                    FRAME_DATA[0] = value;
                    return true;
                default:
                    return false;
            }
        }
        
        /** Sets data in pointer to BSM_SLAVE_RQ
          * 
          * If this function returns false, then the CAN Frame is invalid or has not been seen
          * on the CANBUS network yet. Meaning it's data cannot be used.
          *
          * If the function returns true, then the pointer to 'dest' has been updated with the new CAN data
          */
        bool get_BSM_SLAVE_RQ(uint64_t now, uint64_t max_expire_time, BSM_SLAVE_RQ* dest) const {
            if (LAST_FRAME_TIMES[0] == 0 || dest == nullptr) { // CAN Frame has not been seen on bus yet / NULL pointer
                return false;
            } else if (now - LAST_FRAME_TIMES[0] > max_expire_time) { // CAN Frame has not refreshed in valid interval
                return false;
            } else { // CAN Frame is valid! return it
                dest->raw = FRAME_DATA[0];
                return true;
            }
        }
            
	private:
		uint64_t FRAME_DATA[1];
		uint64_t LAST_FRAME_TIMES[1];
};
#endif // __ECU_BSM_S_H_

#endif // EGS_SPRINTER_MODE