#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "pdf.h"

/*
  ABOUT PDF FILE:
  - A pdf file is a list of bytes (8bit) values called characters.
  - At the lowest level, we make the difference between 'white space', 'delimiter', and 'regular character'
  - White spaces and delimiters helps to chop the string of bytes into tokens
  - One or more tokens can be aggregated into pdf 'Objects'
  - There is a list of valid object type that a pdf may contains (bool, number, string, array, dictionary, etc)
  - Objects may define a 'reference' id which allows quick reference to the said object.

  - TODO(Sam): Complete...

 */


#define PDF_ASSERT assert

#include <assert.h>

enum PDF_BYTE_TYPES_WHITE_SPACE {
	PDF_BYTE_TYPE_WHITE_SPACE_NULL			  = 0x00,
	PDF_BYTE_TYPE_WHITE_SPACE_HORIZONTAL_TAB  = 0x09,
	PDF_BYTE_TYPE_WHITE_SPACE_LINE_FEED		  = 0x0A,
	PDF_BYTE_TYPE_WHITE_SPACE_FORM_FEED		  = 0x0C,
	PDF_BYTE_TYPE_WHITE_SPACE_CARRIAGE_RETURN = 0x0D,
	PDF_BYTE_TYPE_WHITE_SPACE_SPACE			  = 0x20,
};

// If the byte pointed by buffer[*inout_pos] is a white space
// this functionr returns true and set 'inout_pos' to the next
// valid byte which is not a white space.
bool pdf_byte_is_white_space(const uint8_t* buffer, size_t* inout_pos)
{
	bool was_white_space = false;
	while(true) {
		switch(buffer[*inout_pos]) {
		case PDF_BYTE_TYPE_WHITE_SPACE_NULL:
		case PDF_BYTE_TYPE_WHITE_SPACE_HORIZONTAL_TAB:
		case PDF_BYTE_TYPE_WHITE_SPACE_LINE_FEED:
		case PDF_BYTE_TYPE_WHITE_SPACE_FORM_FEED:
		case PDF_BYTE_TYPE_WHITE_SPACE_CARRIAGE_RETURN:
		case PDF_BYTE_TYPE_WHITE_SPACE_SPACE:
		{
			was_white_space = true;
			*inout_pos += 1;
			continue;
		}
		};
		break;
	}
	return was_white_space;
}

enum PDF_BYTE_TYPES_DELIMITER {
	// Special delimiters
	PDF_BYTE_TYPE_DELIMITER_LEFT_PARENTHESIS = 0x28,
	PDF_BYTE_TYPE_DELIMITER_RIGHT_PARENTHESIS = 0x29,
	PDF_BYTE_TYPE_DELIMITER_LESS_THAN_SIGN = 0x3C,
	PDF_BYTE_TYPE_DELIMITER_GREATER_THAN_SIGN = 0x3E,
	PDF_BYTE_TYPE_DELIMITER_LEFT_SQUARE_BRACKET = 0x5B,
	PDF_BYTE_TYPE_DELIMITER_RIGHT_SQUARE_BRACKET = 0x5D,
	PDF_BYTE_TYPE_DELIMITER_SOLIDUS = 0x2F,
	PDF_BYTE_TYPE_DELIMITER_PERCENT_SIGN = 0x25,
	// Additional delimiters (for Type 4 PostScript calculator functions)
	PDF_BYTE_TYPE_DELIMITER_LEFT_CURLY_BRACE = 0x7B,
	PDF_BYTE_TYPE_DELIMITER_RIGHT_CURLY_BRACE = 0x7D,
};

// If the byte pointed by buffer[*inout_pos] is a delimiter
// this function returns true and set 'inout_pos' to the next
// valid byte which is not a delimiter.
bool pdf_byte_is_delimiter(uint8_t* buffer, size_t* inout_pos) {
	switch(buffer[*inout_pos]) {
	case PDF_BYTE_TYPE_DELIMITER_LEFT_PARENTHESIS:
	case PDF_BYTE_TYPE_DELIMITER_RIGHT_PARENTHESIS:
	case PDF_BYTE_TYPE_DELIMITER_LESS_THAN_SIGN:
	case PDF_BYTE_TYPE_DELIMITER_GREATER_THAN_SIGN:
	case PDF_BYTE_TYPE_DELIMITER_LEFT_SQUARE_BRACKET:
	case PDF_BYTE_TYPE_DELIMITER_RIGHT_SQUARE_BRACKET:
	case PDF_BYTE_TYPE_DELIMITER_SOLIDUS:
	case PDF_BYTE_TYPE_DELIMITER_PERCENT_SIGN:
	case PDF_BYTE_TYPE_DELIMITER_LEFT_CURLY_BRACE:
	case PDF_BYTE_TYPE_DELIMITER_RIGHT_CURLY_BRACE:
	{
		*inout_pos += 1;
		return true;
	}
	};
	return false;
}

// This is usefull for testing dbl char construct like << (3C3Ch) or >> (3E3Eh)
bool pdf_byte_is_double_character_construct(const uint8_t* buffer, size_t* inout_pos)
{
	if(buffer[*inout_pos] == buffer[*inout_pos+1])
	{
		*inout_pos += 2;
		return true;
	}
	return false;
}

// Consume an EOL character if present and return true in addition of setting
// 'inout_pos' to the next valid character
// If not present return false and don't change anything
bool pdf_byte_is_end_of_line(const uint8_t* buffer, size_t* inout_pos)
{
	if(buffer[*inout_pos] == PDF_BYTE_TYPE_WHITE_SPACE_LINE_FEED)
	{
		*inout_pos += 1;
		return true;
	}
	else if(buffer[*inout_pos] == PDF_BYTE_TYPE_WHITE_SPACE_CARRIAGE_RETURN
			&& buffer[*inout_pos+1] == PDF_BYTE_TYPE_WHITE_SPACE_LINE_FEED)
	{
		*inout_pos += 2;
		return true;
	}
	return false;
}

// Consume a full comment, starting from '%' up to EOL char but not included
// 'inout_pos' is updated to point to the first valid byte after the comment
// i.e. the EOL char
bool pdf_byte_is_comment(const uint8_t* buffer, size_t* inout_pos)
{
	size_t pos = *inout_pos;
	if(buffer[pos] == PDF_BYTE_TYPE_DELIMITER_PERCENT_SIGN)
	{
		do {
			*inout_pos += 1;
			pos += 1;
		} while(!pdf_byte_is_end_of_line(buffer, &pos));
		
		return true;
	}
	return false;
}

enum PDF_OBJECT_TYPES {
	// The special NONE keyword is not a keyword but is used as
	// a falsy return value.
	PDF_OBJECT_TYPE_NONE = false,
	// Objects from the spec
	PDF_OBJECT_TYPE_NULL,
	PDF_OBJECT_TYPE_BOOLEAN,
	PDF_OBJECT_TYPE_INTEGER,
	PDF_OBJECT_TYPE_REAL,
	PDF_OBJECT_TYPE_STRING,
	PDF_OBJECT_TYPE_NAME,
	PDF_OBJECT_TYPE_ARRAY,
	PDF_OBJECT_TYPE_DICTIONARY,
	PDF_OBJECT_TYPE_STREAM,
};

enum PDF_KEYWORDS {
	// The special NONE keyword is not a keyword but is used as
	// a falsy return value.
	PDF_KEYWORD_NONE = false,
	// TODO(Sam): I'll complete this list as I go trough the spec
	PDF_KEYWORD_TRUE,
	PDF_KEYWORD_FALSE,
};

// For reading it we can allow large bits count for compatibility
// For writing it would we should follow spec and use 32bits
#define PDF_INTEGER_TYPE int64_t
#define PDF_REAL_TYPE double

typedef struct {
	char* start;
	size_t length;
} PdfString;


typedef struct {
	int type;
	union {
		bool bool_value;
		PDF_INTEGER_TYPE int_value;
		PDF_REAL_TYPE real_value;
		PdfString string_value;
	};
} PdfObject;

typedef struct {
	size_t chunk_id, pos_start;
	size_t pos_end; // One after last element
} PdfToken;

// Tries to consume a keyword.
// If it succeed it returns it and update 'inout_pos' to point to the next byte
// If don't succeed returns PDF_KEYWORD_NONE
int pdf_try_to_consume_keyword(const uint8_t* buffer, PdfToken token)
{
	size_t token_len = token.pos_end - token.pos_start;
	char* str = (char*)(&buffer[token.pos_start]);	

	if(strncmp(str, "true", token_len) == 0)
	{
		return PDF_KEYWORD_TRUE;
	}
	if(strncmp(str, "false", token_len) == 0)
	{
		return PDF_KEYWORD_FALSE;
	}

	return PDF_KEYWORD_NONE;
}

// Tries to consume a number (int or real) and update inout_obj if it succeed.
// Return true or false if succeeded or not.
bool pdf_try_to_consume_number(const uint8_t* buffer, PdfToken token, PdfObject* inout_obj)
{
	int sign = 1;
	uint8_t start = token.pos_start;
	uint8_t end = token.pos_end;
	if(buffer[start] == '+')
		++start;
	else if(buffer[start] == '-')
	{
		sign = -1;
		++start;
	}
	
	bool is_real_number = false;
	size_t nb_of_tens = 0;
	for(size_t i = start; i < end; ++i)
	{
		if(buffer[i] == '.') { is_real_number = true; break; }
		if(buffer[i] < '0' || buffer[i] > '9') return false;
		nb_of_tens += 1;
	}
	
	// TODO(Sam): Overflow check?
	PDF_INTEGER_TYPE tens = 1;
	PDF_INTEGER_TYPE int_value = 0;
	for(size_t j = start + nb_of_tens - 1; start <= j; --j)
	{
		PDF_ASSERT(buffer[j] >= '0' && buffer[j] <= '9' && "Try to read a non digit character as a number");
		int_value += tens*(buffer[j]-'0');
		tens *= 10;
	}

	if(is_real_number)
	{
		PDF_REAL_TYPE one_over_tens = (PDF_REAL_TYPE)1.0;
		PDF_REAL_TYPE real_value = (PDF_REAL_TYPE)int_value;
		for(size_t j = start + nb_of_tens + 1; j < end; ++j)
		{
			if(buffer[j] < '0' || buffer[j] > '9') return false;
			one_over_tens /= (PDF_REAL_TYPE)10.0;
			real_value += one_over_tens*(PDF_REAL_TYPE)(buffer[j]-'0');
		}
		inout_obj->type = PDF_OBJECT_TYPE_REAL;
		inout_obj->real_value = sign*real_value;
	}
	else
	{
		inout_obj->type = PDF_OBJECT_TYPE_INTEGER;
		inout_obj->int_value = sign*int_value;
	}
	
	return true;
}

void debug_pdf_print_object(PdfObject* obj)
{
	if(obj->type)
	{
		switch(obj->type)
		{
		case PDF_OBJECT_TYPE_BOOLEAN:
		{
			printf("Boolean value: %s\n", obj->bool_value ? "TRUE" : "FALSE");
		} break;
		case PDF_OBJECT_TYPE_INTEGER:
		{
			printf("Integer value: %lli\n", obj->int_value);
		} break;
		case PDF_OBJECT_TYPE_REAL:
		{
			printf("Real value: %lf\n", obj->real_value);
		} break;
		case PDF_OBJECT_TYPE_STRING:
		{
			printf("String len: %zu, value: ", obj->string_value.length);
			for(size_t i = 0; i < obj->string_value.length; ++i)
			{
				printf("%c", obj->string_value.start[i]);
			}
			printf("\n");
		} break;
		}
	}
}

bool pdf_parse_literal_string(const uint8_t* buffer, size_t* inout_pos, size_t buffer_len,
							  PdfObject* inout_obj)
{
	inout_obj->type = PDF_OBJECT_TYPE_STRING;
	inout_obj->string_value.start = NULL;
	inout_obj->string_value.length = 0;
	
	size_t pos = *inout_pos;
	if(buffer[pos] != '(') return false;

	++pos; // We start by estimating string length for memory allocation
	int parenthesis_count = 1;
	while(pos < buffer_len) {
		if(buffer[pos] == '(') ++parenthesis_count;
		if(buffer[pos] == ')') --parenthesis_count;
		if(parenthesis_count <= 0) break; // End of string

		// TODO(Sam): Here we have a bug if we end the data
		//            stream with a '\'. We will refactor
		//            this buffer system at some point so
		//            I won't spend time now fixing it...
		
		// We must escape \( and \) sequences
		if(buffer[pos] == '\\' && buffer[pos+1] == '(') ++pos;
		if(buffer[pos] == '\\' && buffer[pos+1] == ')') ++pos;

		++pos;
	}
	if(parenthesis_count != 0) return false;
	size_t tmp_pos = *inout_pos + 1;
	inout_obj->string_value.length = pos - *inout_pos - 1;	// - 1 for removing first '('
	*inout_pos	   = pos + 1;	// + 1 for removing last ')'
	pos = tmp_pos;

	// Note(Sam): At this point, we did not processed escaped characters yet
	//            so the reserved memory may be slightly larger than the
	//            final string length. This is okay, memory is cheap nowdays
	//            and we will correct for it at the end.
	if(inout_obj->string_value.length == 0) return true;
	inout_obj->string_value.start = (char*)malloc(inout_obj->string_value.length*sizeof(char));
	if(inout_obj->string_value.start == NULL)
	{
		PDF_ASSERT(false && "TODO: Repport memory allocation error!");
	}

	size_t next_i = 0;
	for(size_t i = next_i; i < inout_obj->string_value.length; ++i)
	{
		if(buffer[pos+i] == '\\')
		{
			// TODO(Sam): Here we have a bug if we end the data
			//            stream with a '\'. We will refactor
			//            this buffer system at some point so
			//            I won't spend time now fixing it...
			switch(buffer[pos+i+1]) {
			case 'n': inout_obj->string_value.start[next_i]	= 0x0A; ++i; break;
			case 'r': inout_obj->string_value.start[next_i]	= 0x0D; ++i; break;
			case 't': inout_obj->string_value.start[next_i]	= 0x09; ++i; break;
			case 'b': inout_obj->string_value.start[next_i]	= 0x08; ++i; break;
			case 'f': inout_obj->string_value.start[next_i]	= 0x0C; ++i; break;
			case '(': inout_obj->string_value.start[next_i]	= '(';  ++i; break;
			case ')': inout_obj->string_value.start[next_i]	= ')';  ++i; break;
			case '\\': inout_obj->string_value.start[next_i] = '\\'; ++i; break;
			default:
			{
				size_t np = pos+i+1;
				if(pdf_byte_is_end_of_line(buffer, &np))
				{
					// Multiline splited string
					// We chopped the EOL character
					i = np - pos - 1;
					--next_i; // NOTE(Sam): We decrement next_i since we increment it later
					          //            and we should not change it in this case!
				}
				else  if(buffer[pos+i+1] >= '0' && buffer[pos+i+1] <= '7')
				{
					uint8_t len = 1;
					if(buffer[pos+i+2] >= '0' && buffer[pos+i+2] <= '7') ++len;
					if(buffer[pos+i+3] >= '0' && buffer[pos+i+3] <= '7') ++len;

					uint16_t value = 0;
					uint16_t exp = 1;
					for(size_t j = 0; j < len; ++j)
					{
						value += exp*(buffer[pos+i+len-j] - '0');
						exp *= 8;
					}
					value &= 255;
					inout_obj->string_value.start[next_i] = (char)value;
					i += len;
				}
				else
				{
					// Nothing else to do here
					// In this last case, the \ char must be ignored
					--next_i; // NOTE(Sam): We decrement next_i since we increment it later
					          //            and we should not change it in this case!
				}
			}
			}
		}
		else
		{
			inout_obj->string_value.start[next_i] = buffer[pos+i];
		}

		++next_i;
	}
	inout_obj->string_value.length = next_i;

	return true;
}

bool pdf_parse_hexadecimal_string(const uint8_t* buffer, size_t* inout_pos, size_t buffer_len,
								  PdfObject* inout_obj)
{
	inout_obj->type = PDF_OBJECT_TYPE_STRING;
	inout_obj->string_value.start = NULL;
	inout_obj->string_value.length = 0;

	size_t pos = *inout_pos;
	if(buffer[pos] != '<') return false;

	++pos; // We start by estimating string length for memory allocation
	int parenthesis_count = 1;
	while(pos < buffer_len) {
		if(buffer[pos] == '>') break;

		size_t np = pos;
		if( !(
			(buffer[pos] >= '0' && buffer[pos] <= '9') ||
			(buffer[pos] >= 'a' && buffer[pos] <= 'f') ||
			(buffer[pos] >= 'A' && buffer[pos] <= 'F')
				)
			&& !pdf_byte_is_white_space(buffer, &np))
		{
			// NOTE(Sam): Error if not 0-9A-F
			return false;
		}
		
		// TODO(Sam): Here we have a bug if we end the data
		//            stream with a '\'. We will refactor
		//            this buffer system at some point so
		//            I won't spend time now fixing it...
		
		++pos;
	}
	size_t tmp_pos = *inout_pos + 1;
	inout_obj->string_value.length = pos - *inout_pos - 1;	// - 1 for removing first '<'
	*inout_pos	   = pos + 1;	// + 1 for removing last '>'
	pos = tmp_pos;

	// Note(Sam): At this point, we did not processed white characters yet
	//            so the reserved memory may be slightly larger than the
	//            final string length. Moreover, two characters make one byte
	//            so the final string will be len/2 + 1 (+1 for odd len)
	if(inout_obj->string_value.length == 0) return true;
	inout_obj->string_value.start = (char*)malloc((inout_obj->string_value.length/2+1)*sizeof(char));
	if(inout_obj->string_value.start == NULL)
	{
		PDF_ASSERT(false && "TODO: Repport memory allocation error!");
	}

	uint8_t dig_id = 0;
	uint8_t digits[2] = {0};
	size_t next_id = 0;
	for(size_t i = next_id; i < inout_obj->string_value.length; ++i)
	{
		size_t np = pos + i;
		if(pdf_byte_is_white_space(buffer, &np))
		{
			// Multiline splited string
			// We chopped the EOL character
			i = np - pos - 1;
		}
		else
		{
			if(buffer[pos+i] >= '0' && buffer[pos+i] <= '9')
				digits[dig_id] = buffer[pos+i] - '0';
			else if(buffer[pos+i] >= 'a' && buffer[pos+i] <= 'f')
				digits[dig_id] = buffer[pos+i] - 'a' + 10;
			else if(buffer[pos+i] >= 'A' && buffer[pos+i] <= 'F')
				digits[dig_id] = buffer[pos+i] - 'A' + 10;

			if(dig_id == 1)
			{
				inout_obj->string_value.start[next_id] = 16*digits[0];
				inout_obj->string_value.start[next_id] += digits[1];
				++next_id;
			}
			
			dig_id = (dig_id+1)%2;
		}
	}
	if(dig_id == 1)
	{
		inout_obj->string_value.start[next_id] = 16*digits[0];
		++next_id;
	}
	inout_obj->string_value.length = next_id;

	return true;
}

void pdf_parse_token(const uint8_t* buffer, PdfToken token, PdfToken* tokens, size_t* next_token_id)
{
	if(token.pos_start >= token.pos_end) return;
	printf("Token %zu: ", *next_token_id);
	for(size_t j = token.pos_start; j < token.pos_end; ++j)
	{
		printf("%c", buffer[j]);
	}
	printf("\n");
	
	tokens[(*next_token_id)++] = token;

	PdfObject obj = {.type = PDF_OBJECT_TYPE_NONE};

	int keyword;
	if(pdf_try_to_consume_number(buffer, token, &obj)) {
		debug_pdf_print_object(&obj);
	}
	else if(keyword = pdf_try_to_consume_keyword(buffer, token)) {
		printf("> Found the keyword: %i\n", keyword);
		switch(keyword) {
		case PDF_KEYWORD_TRUE:
		{
			obj.type = PDF_OBJECT_TYPE_BOOLEAN;
			obj.bool_value = true;
		} break;
		case PDF_KEYWORD_FALSE:
		{
			obj.type = PDF_OBJECT_TYPE_BOOLEAN;
			obj.bool_value = false;
		} break;
		}
		debug_pdf_print_object(&obj);
	}
}

int main( void ) {

	//char* filename = "test01.pdf";
	char* filename = "test02.pdf";
	//char* filename = "example.pdf";

	FILE *file = NULL;
	file = fopen(filename, "rb");
	if(file == NULL)
	{
		printf("ERROR: Could not open file '%s'\n", filename);
		exit(1);
	}

	const size_t CHUNK_SIZE = 4096;
	size_t chunk_id = 0;
	size_t pos = 0; // Absolute pos is chunk_id*CHUNK_SIZE + pos
	size_t prev_pos, next_pos;
	size_t token_start = 0;
	size_t nb_bytes_readed = 0;

	uint8_t* buffer  = (uint8_t*)malloc(CHUNK_SIZE*sizeof(uint8_t));
	if(buffer == NULL)
	{
		printf("ERROR: Not enough memory...\n");
		fclose(file);
		exit(1);
	}

	size_t next_token_id = 0;
	PdfToken tokens[2048] = {0};
	PdfToken current_token = {0};
	bool chopped_a_token = false;
	
	// TODO(Sam): At some point we will need a better way to go trough
	//            file and to request specific chunks.
	// TODO(Sam): This will crash if multi bytes character are at the end
	//            of the buffer...
	if((nb_bytes_readed = fread(buffer, 1, CHUNK_SIZE, file)) > 0 )
	{
		printf("Readed %zu bytes in buffer!\n", nb_bytes_readed);
		current_token.chunk_id = 0;
		for(pos = 0; pos < nb_bytes_readed; )
		{
			chopped_a_token = false;
			prev_pos = pos;
			next_pos = pos;
			current_token.pos_end = pos;
			if(pdf_byte_is_comment(buffer, &next_pos)) {
				//printf("Comment from byte %zu to %zu!\n", pos, next_pos);
				chopped_a_token = true;
				pos = next_pos;
			}
			else if(pdf_byte_is_white_space(buffer, &next_pos)) {
				//printf("Byte %zu: %u is a white space of len %zu.\n", pos, buffer[pos], next_pos - pos);
				chopped_a_token = true;
				pos = next_pos;
			}
			else if(pdf_byte_is_delimiter(buffer, &next_pos)) {
				switch(buffer[pos])
				{
				case '(':
				{
					size_t np = pos;
					PdfObject obj;
					if(!pdf_parse_literal_string(buffer, &np, nb_bytes_readed, &obj))
					{
						PDF_ASSERT(false && "TODO: Repport parsing error!");
					}
					debug_pdf_print_object(&obj);
					next_pos = np;
				} break;
				case '<':
				{
					size_t np = pos;
					PdfObject obj;
					if(!pdf_parse_hexadecimal_string(buffer, &np, nb_bytes_readed, &obj))
					{
						PDF_ASSERT(false && "TODO: Report parsing error!");
					}
					debug_pdf_print_object(&obj);
					next_pos = np;
				} break;
				}
				chopped_a_token = true;
				pos = next_pos;
			}
			else {
				//printf("Just a standard character: %c\n", (char)buffer[pos]);
				pos += 1;
			}

			if(pos == nb_bytes_readed) chopped_a_token = true;
			
			if(chopped_a_token)
			{
				pdf_parse_token(buffer, current_token, tokens, &next_token_id);
				current_token.pos_start = pos;
			}
			PDF_ASSERT(pos != prev_pos && "ERROR: We did not move forward. Forgot to set 'pos' or increment it?");
		}
	}

	/* printf("Tokens:\n"); */
	/* for(size_t i = 0; i < next_token_id; ++i) */
	/* { */
	/* 	printf("Token %zu: ", i); */
	/* 	for(size_t j = tokens[i].pos_start; j < tokens[i].pos_end; ++j) */
	/* 	{ */
	/* 		printf("%c", buffer[j]); */
	/* 	} */
	/* 	printf("\n"); */
	/* } */
	
	
	free(buffer);
	fclose(file);
	return 0;
	
}
