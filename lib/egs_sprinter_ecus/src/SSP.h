
/**
* AUTOGENERATED BY convert.py
* DO NOT EDIT THIS FILE!
*
* IF MODIFICATIONS NEED TO BE MADE, MODIFY can_data.txt!
*
* CAN Defintiion for ECU 'SSP'
*/

#ifdef EGS_SPRINTER_MODE

#ifndef __ECU_SSP_H_
#define __ECU_SSP_H_

#include <stdint.h>
    
#define SSP_RS_SSP_CAN_ID 0x0381

/** Actual Current Duty Cycle */
enum class SSP_RS_SSP_CurDtyCyc_Actl {
	PASSIV = 0, // unknown
	VOR = 1, // unknown
	RUECK = 2, // unknown
	SNV = 3, // unknown
};

/** SSP Diagnostics State */
enum class SSP_RS_SSP_SSP_Diag_Stat {
	SNV = 255, // unknown
};



typedef union {
	uint64_t raw;
	uint8_t bytes[8];

	/** Gets CAN ID of SSP_RS_SSP */
	uint32_t get_canid(){ return SSP_RS_SSP_CAN_ID; }
    /** Sets Actual Current Duty Cycle */
    void set_CurDtyCyc_Actl(SSP_RS_SSP_CurDtyCyc_Actl value){ raw = (raw & 0x00ffffffffffffff) | ((uint64_t)value & 0xff) << 56; }

    /** Gets Actual Current Duty Cycle */
    SSP_RS_SSP_CurDtyCyc_Actl get_CurDtyCyc_Actl() const { return (SSP_RS_SSP_CurDtyCyc_Actl)(raw >> 56 & 0xff); }
        
    /** Sets SSP Diagnostics State */
    void set_SSP_Diag_Stat(SSP_RS_SSP_SSP_Diag_Stat value){ raw = (raw & 0xff00ffffffffffff) | ((uint64_t)value & 0xff) << 48; }

    /** Gets SSP Diagnostics State */
    SSP_RS_SSP_SSP_Diag_Stat get_SSP_Diag_Stat() const { return (SSP_RS_SSP_SSP_Diag_Stat)(raw >> 48 & 0xff); }
        
    /** Sets Message Counter. Conversion formula (To raw from real): y=(x-0.0)/1.00 */
    void set_MC_SSP_RS_SSP(uint8_t value){ raw = (raw & 0xffffffffffff0fff) | ((uint64_t)value & 0xf) << 12; }

    /** Gets Message Counter. Conversion formula (To real from raw): y=(1.00x)+0.0 */
    uint8_t get_MC_SSP_RS_SSP() const { return (uint8_t)(raw >> 12 & 0xf); }
        
    /** Sets CRC checksum byte 1 - 7 to SAE J1850. Conversion formula (To raw from real): y=(x-0.0)/1.00 */
    void set_CRC_SSP_RS_SSP(uint8_t value){ raw = (raw & 0xffffffffffffff00) | ((uint64_t)value & 0xff) << 0; }

    /** Gets CRC checksum byte 1 - 7 to SAE J1850. Conversion formula (To real from raw): y=(1.00x)+0.0 */
    uint8_t get_CRC_SSP_RS_SSP() const { return (uint8_t)(raw >> 0 & 0xff); }
        
} SSP_RS_SSP;



class ECU_SSP {
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
                case SSP_RS_SSP_CAN_ID:
                    LAST_FRAME_TIMES[0] = timestamp_now;
                    FRAME_DATA[0] = value;
                    return true;
                default:
                    return false;
            }
        }
        
        /** Sets data in pointer to SSP_RS_SSP
          * 
          * If this function returns false, then the CAN Frame is invalid or has not been seen
          * on the CANBUS network yet. Meaning it's data cannot be used.
          *
          * If the function returns true, then the pointer to 'dest' has been updated with the new CAN data
          */
        bool get_SSP_RS_SSP(uint64_t now, uint64_t max_expire_time, SSP_RS_SSP* dest) const {
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
#endif // __ECU_SSP_H_

#endif // EGS_SPRINTER_MODE