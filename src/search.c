#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>
#include <glib.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>
#include <regex.h>
/* Luc A since janv 2018 in order to parse Office files */

#include <errno.h>
#include <assert.h>
#include <zip.h>
#include <poppler.h>
#include <string.h>
/* end Luc A */

#include "interface.h" /* glade requirement */
#include "support.h" /* glade requirement */
#include "search.h" /* Local headers + global stuff */
#include "savestate.h" /* library to save/restore config.ini settings */
#include "regexwizard.h" /* regular expression builder wizard */
#include "systemio.h" /* System stuff, file import and export code */
#include "misc.h" /* Everything else */

/* external global var sorry */
extern gboolean fStartedSearch;
/*mutexes for searchdata and searchControl->cancelSearch */
static GMutex mutex_Data; /* Global mutex used by savestate.c too */
static GMutex mutex_Control; /* Global mutex used by savestate.c too*/

/* it's very easy to improve the quality by changing the field icon_file_name */
static t_symstruct lookuptable[] = {
    { "txt-", "icon-text-generic.png", FORMAT_TXT }, 
    { "odt-", "icon-odt.png", FORMAT_OFFICE_TEXT }, 
    { "docx","icon-doc.png", FORMAT_OFFICE_TEXT }, /* we keep only 4 first chars */
    { "rtf-", "icon-word-processor.png", FORMAT_OFFICE_TEXT },
    { "doc-", "icon-doc.png", FORMAT_OFFICE_TEXT }, /* 5 */
    { "abw-", "icon-word-processor.png", FORMAT_OFFICE_TEXT },
    { "ods-", "icon-ods.png", FORMAT_OFFICE_SHEET }, 
    { "xlsx", "icon-xls.png", FORMAT_OFFICE_SHEET }, 
    { "xls-", "icon-xls.png", FORMAT_OFFICE_SHEET },
    { "odb-", "icon-odb.png", FORMAT_OFFICE_BASE },/* 10*/ 
    { "png-", "icon-image.png", FORMAT_IMAGE },  
    { "jpg-", "icon-image.png", FORMAT_IMAGE }, 
    { "jpeg", "icon-image.png", FORMAT_IMAGE }, 
    { "xcf-", "icon-image.png", FORMAT_IMAGE },
    { "pdf-", "icon-pdf.png", FORMAT_PDF },/* 15 */
    { "svg-", "icon-drawing.png", FORMAT_SVG },
    { "odg-", "icon-odg.png", FORMAT_ODG },
    { "csv-", "icon-spreadsheet.png", FORMAT_CSV },
    { "zip-", "icon-archive.png", FORMAT_ZIP },
    { "wav-", "icon-audio.png", FORMAT_WAV },/* 20*/
    { "mp3-", "icon-audio.png", FORMAT_MP3 },
    { "mp4-", "icon-video.png", FORMAT_MP4 },
    { "avi-", "icon-video.png", FORMAT_AVI },
    { "mkv-", "icon-video.png", FORMAT_MKV },
    { "otf-", "icon-font.png", FORMAT_OTF },/* 25 */
    { "ttf-", "icon-font.png", FORMAT_TTF },
    { "bz2-", "icon-archive.png", FORMAT_BZ2 },
    { "ppt-", "icon-ppt.png", FORMAT_PPT },
    { "deb-", "icon-archive.png", FORMAT_DEB },
    { "gz-t", "icon-archive.png", FORMAT_GZ },/* 30 */
    { "rpm-", "icon-archive.png", FORMAT_RPM },
    { "sh-t", "icon-java.png", FORMAT_SH },
    { "c-ty", "icon-c.png", FORMAT_C },
    { "xml-", "icon-htm.png", FORMAT_XML },
    { "htm-", "icon-htm.png", FORMAT_HTML },/* 35 */
    { "jar-", "icon-java.png", FORMAT_JAR },
    { "java", "icon-java.png", FORMAT_JAVA },
    { "h-ty", "icon-h.png", FORMAT_H },
    { "rar-", "icon-archive.png", FORMAT_RAR },
    { "tif-", "icon-image.png", FORMAT_TIFF }, /* 40*/
    { "dng-", "icon-image.png", FORMAT_DNG },
    { "gif-", "icon-image.png", FORMAT_GIF },
    { "odp-", "icon-odp.png", FORMAT_ODP },
    { "js-t", "icon-java.png", FORMAT_JS },
    { "css-", "icon-htm.png", FORMAT_CSS },/* 45 */
    { "tgz-", "icon-archive.png", FORMAT_TGZ },
    { "xpm-", "icon-image.png", FORMAT_XPM },
    { "cpp-", "icon-c.png", FORMAT_C },
    { "html", "icon-htm.png", FORMAT_HTML },
    { "sql-", "icon-odb.png", FORMAT_OFFICE_BASE }, /* 50 */
    { "unknown-type", "icon-unknown.png", FORMAT_OTHERS }
};
// please update in search.h the #define MAX_FORMAT_LIST according to the size of this table - Luc A 1 janv 2018
#define NKEYS ( sizeof (lookuptable)/ sizeof(t_symstruct) )

/********************************************
  convert a non-UTF8 string using 
  Glib functions
  Dirty : all chars <32 are removed
 ********************************************/
gchar *MSWordconvert_str(unsigned char *buffer, gint fromCP, glong len)
{
  glong i, count;
  GError **error;
  glong bytes_written, bytes_read;
  gchar *sRetVal=NULL;
  GString *str=g_string_new("");

  count = 1;
  if(fromCP==iCpUtf16)
    count = 2;
  i=0;
  while(i<len) {
    /* we have to switch between 8bits or 16 bits chars codings */
     if(buffer[i]<32) {
       switch(buffer[i]) {
         case 0x0D:
         case 12:/* page break */
         case 14:/* column break */
         case 11:{
           str= g_string_append (str,"\n");
           break;
         }
         case 0x19:{
           str= g_string_append (str,"'");
           break;
         }
         case 31:
         case 30:{
           str= g_string_append (str,"-");
           break;
         }
         default:{
           str= g_string_append (str," ");
         }
       }/* end switch <32 */          
     }
     else {
       switch(fromCP) {
         case iCpUtf16: {
            str= g_string_append (str,g_convert_with_fallback ((gchar *)buffer+i, 2, 
                                    "UTF8", "UTF16", NULL, &bytes_read, &bytes_written, &error));
            break;
         }
         case iCpIbm437: {
            str = g_string_append (str,g_convert_with_fallback ((gchar *)buffer+i, 1, "UTF8", "CP437",
                                           NULL, &bytes_read, &bytes_written, &error));
            break;
         }
         case iCpIso8859_1: {
            str = g_string_append (str,g_convert_with_fallback ((gchar *)buffer+i, 1, "UTF8", "ISO-8859-1",
                                           NULL, &bytes_read, &bytes_written, &error));
            break;
         }
         default: {
           str= g_string_append (str,g_convert_with_fallback ((gchar *)buffer+i, 1, 
                                    "UTF8", "WINDOWS-1252", NULL, &bytes_read, &bytes_written, &error));         
         }
       }/* end switch */
     }
    i=i+count;
  }/* wend i */
  sRetVal = g_strdup_printf("%s", str->str);
  g_string_free(str, TRUE);
  return sRetVal;
}
/*********************************************
  ms-Word OLE (W6/95/97>>2003 parser
*********************************************/
gchar *OLECheckFile(gchar *path_to_file, gchar *path_to_tmp_file)
{
  FILE *outputFile, *inputFile;
  GError **error;
  glong fileSize, bytes_written, bytes_read;
  const gint MSAT_ORIG_SIZE = 436;
  gint i, j, k, fWordFlags, typeEntry, charset, wordVersion;
  glong currentID, count, rootDirectorySectId, WordDocumentSectId, tmpSecId, tmpByteslength, fileOffset, SATValue;
  unsigned char *SAT = NULL, *SSAT = NULL;
  unsigned char *SSCS = NULL, *directoryChain = NULL, *WordDocument = NULL, *clx = NULL, *pieceTable = NULL;
  unsigned char *tableStream = NULL, pieceDescriptor[8];
  gchar *str=NULL, *buffer;
  gint countDirectoryChain =0,sectId1TableStream = -10, sectId0TableStream = -10, sectIdTableStream=-10; /* arbitrary for me */
  glong WordDocumentStreamLen, fcClx=0, lcbClx=0, lcbPieceTable =0, usedSect, secIdDirectoryStream, minSizeSectorStream, secIdFirstSSAT;
  glong ssat_size, secIdFirstSectorMSAT, msat_size, textstart, textlen, fcValue, fcOffset;
  gint fTableStream = -1, TableStreamLen = 0, sectorSize,  shortSectorSize, pieceCount, offsetPieceDescriptor;
  gboolean goOn = TRUE, fWordDocument = FALSE,fCreatedMac = FALSE;
  gchar end_sign[]={0x0a,0};

  printf("MsWord OLE Parser =%s\n",path_to_file );
  /* the Winword file is binary */
  inputFile = fopen(path_to_file,"rb");
  if(inputFile==NULL) {
          printf("* ERROR : impossible to open MS Winword file:%s *\n", path_to_file);
          return NULL;
  }
  /* we compute the size before dynamically allocate buffer */
  glong prev = ftell(inputFile);   
  fseek(inputFile, 0L, SEEK_END);
  glong sz = ftell(inputFile);
  fseek(inputFile, prev, SEEK_SET);
  /* we allocate the buffer */
  if(sz<=0)
    return NULL;
  buffer = g_malloc0(sz*sizeof(gchar)+sizeof(gchar));
  if(buffer==NULL)
    return NULL;
  /* we start the file reading in Binary mode : it's better for future parsing */
  fileSize = fread(buffer, sizeof(gchar), sz, inputFile);
  fclose(inputFile);
  /* the word coding MUST be in little endian ! */
  if(!(buffer[28]=0xFE)&&(buffer[29]==0xFF)) {
        printf("* can't proceed : file coded in Big endian mode ! \n");
        g_free(buffer);
        return NULL;
  }
  /* sizes */
  msat_size=getlong(buffer,0x48);
  ssat_size = getlong(buffer, 0x40);
  if((msat_size<0) || (ssat_size<0)) {
          g_free(buffer);
          return NULL;
  }
  sectorSize = 1<<getshort(buffer,0x1e);
  shortSectorSize=1<<getshort(buffer,0x20);
  usedSect = getulong(buffer,44);
  /* we allocate memory to store all the SAT used */
  SAT = g_malloc0(usedSect*sectorSize);
  /* directory */
  secIdDirectoryStream = getulong(buffer,48);
  /* SSAT */
  minSizeSectorStream = getulong(buffer, 56);
  secIdFirstSSAT = getlong(buffer, 0x3C);
  /* we allocate memory for the SSAT */
  SSAT = g_malloc0(ssat_size*sectorSize);
  secIdFirstSectorMSAT = getlong(buffer,0x44);

  /* first sector starts at offset 512 */
  /* we must build the Sector Allocation Table (SAT) by reading the MSAT and the sectors pointed by the MSAT */
  i=usedSect;
  count = 76;
  j=0;
  while(i>0) {
          currentID = getlong(buffer, count);
          memcpy (SAT+(j*sectorSize), buffer+512+sectorSize*currentID, sectorSize);
          i--;
          j++;
          count = count+4;
  }/* wend msat_size */
  /* secIdFirstSSAT = first sector of SSAT but the chains is stored in the MAIN sat at secIdFirstSSAT position ! */
  i = ssat_size;
  j = 0;
  count = secIdFirstSSAT;
  while(i>0) {
          memcpy (SSAT+j*sectorSize, buffer+512+count*sectorSize, sectorSize);
          currentID = getlong(SAT, 4*count); 
          count = currentID;
          i--;
          j++;        
  }/* wend ssat_size */
  /* we build the directory chain  */
  if((secIdDirectoryStream<1) || (secIdDirectoryStream>usedSect*sectorSize)) {
          g_free(SAT);
          g_free(SSAT);
          g_free(buffer);
          return NULL;
  }

  SATValue = secIdDirectoryStream;                         
  while((SATValue!=-2) && (SATValue!=0)) {
            directoryChain = g_realloc(directoryChain, (countDirectoryChain+1)*sectorSize);
            memcpy (directoryChain+(countDirectoryChain*sectorSize),buffer+512+(sectorSize*SATValue), sectorSize);
            countDirectoryChain++;
            SATValue = getlong(SAT, 4*SATValue);
  }/* wend satvalue */
  /* we check if the directory chain contains required streams */
  for(i=0; i<countDirectoryChain*4; i++) {
              k = 0x00FF & getshort(directoryChain, 66+(i*128));
              switch(k) {
                case 5:{ 
                   rootDirectorySectId = getlong(directoryChain, 116+(i*128));
                   if(rootDirectorySectId<1) {
                      g_free(SAT);
                      g_free(SSAT);
                      g_free(directoryChain);
                      g_free(buffer);
                      return NULL;
                   }
                   /* now we can build the SSCS */                   
                   j=0;
                   count = 0;
                   tmpSecId = rootDirectorySectId;
                   while(tmpSecId!=-2) {
                     SSCS = g_realloc(SSCS, (count+1)*sectorSize);
                     memcpy(SSCS+(count*sectorSize),buffer+512+ (tmpSecId*sectorSize) , sectorSize);
                     tmpSecId = getlong(SAT, tmpSecId*4);
                     count++;
                   }
                   break;
                }/* end case 5 */
                case 2: case 1:{
                   if(directoryChain[i*128]<32)
                       break; 
                   str = g_convert_with_fallback ((gchar *)(directoryChain)+(i*128), 32, "UTF8", "UTF16",
                                           NULL, &bytes_read, &bytes_written, &error);/* with a maxlen of 32, we are sure that it wil not be any mismatch, with, for example, PPT streams */
                   tmpSecId = getlong(directoryChain, 116+(i*128));
                   tmpByteslength = getlong(directoryChain, 120+(i*128));
                   if(g_ascii_strncasecmp (str,"1Table",12*sizeof(gchar))==0 ) {
                     sectId1TableStream = tmpSecId;
                     sectIdTableStream = tmpSecId;
                     fTableStream = 1;
                     TableStreamLen = tmpByteslength;
                   }
                   if(g_ascii_strncasecmp (str,"0Table",12*sizeof(gchar))==0 ) {
                     sectId0TableStream = tmpSecId;
                     sectIdTableStream = tmpSecId;
                     fTableStream =0;
                     TableStreamLen = tmpByteslength;
                   }
                   if(g_ascii_strncasecmp (str,"WordDocument",12*sizeof(gchar))==0 ) {
                     fWordDocument = TRUE;
                     WordDocumentSectId = tmpSecId;
                     WordDocumentStreamLen = tmpByteslength;
                   }
                   g_free(str);
                   break;
                }
                default:;                   
              }/* end switch */         
  }/* next i */
  if( (!fWordDocument)|| (WordDocumentStreamLen>fileSize)) {
     printf("* OLE file, but NOT a Word File ! *\n");
     g_free(SAT);
     g_free(SSAT);
     g_free(directoryChain);
     g_free(SSCS);
     g_free(buffer);
     return NULL;
  }
  /* we build the WordDocument stream */
  count = 0;
  /* if fWordDocument is in the SSCS ? */
  if(WordDocumentStreamLen<minSizeSectorStream) {
     while(WordDocumentSectId!=-2) {
         WordDocument = g_realloc(WordDocument, (count+1)*shortSectorSize);       
         memcpy(WordDocument+count*shortSectorSize, SSCS+(WordDocumentSectId*shortSectorSize), shortSectorSize);
         WordDocumentSectId = getlong(SSAT, WordDocumentSectId*4);
         count++;
     }/* wend */
  }
  else {/* datas in the SAT */
       while(WordDocumentSectId!=-2) {
            WordDocument = g_realloc(WordDocument, (count+1)*sectorSize);
            memcpy(WordDocument+count*sectorSize, buffer+512+(WordDocumentSectId*sectorSize), sectorSize);
            WordDocumentSectId = getlong(SAT, WordDocumentSectId*4);
            count++;
       }/*wend*/
  }

  wordVersion = getshort(WordDocument,0);   
  fWordFlags = getshort(WordDocument,10);

  if((WordDocument==NULL) || ((wordVersion!=fWord6 ) && (wordVersion!=fWord95) && (wordVersion!=fWord97) )) {
                             if(WordDocument)
                                g_free(WordDocument);
                             g_free(SAT);
	                     g_free(SSAT);
                             g_free(directoryChain);
                             g_free(SSCS);
                             g_free(buffer);
                             return NULL;
  }

  if((wordVersion==fWord97)&&(fTableStream<0)){  
                             g_free(SAT);
	                     g_free(SSAT);
                             g_free(directoryChain);
                             g_free(SSCS);
                             g_free(WordDocument);
                             g_free(buffer);
                             return NULL;
  }

  /* now we have a W6/95/97 document */
  textstart=getlong(WordDocument,24);/* fib.fcMin */
  textlen=getlong(WordDocument,28)-textstart;/* fib.fcMac */
  if((wordVersion ==fWord6) ||  (wordVersion==fWord95)) {
            if (fWordFlags & fComplex) {
             /* if the file has been 'fast saved' we must access to the piece table */
             /* we get infos for 'fast saved' documents because they have various 'pieces' of datas - 
                thanks to the antiword ideas ! in W6/95, there isn't any PieceTable stream, but the equivalent starts at offset */
                  fcClx = getlong(WordDocument, 0x160);/* fcClx in W6/7 docs - ile of offset of beginning of information 
                          for   complex files. Consists of an encoding of all of the prms quoted by the document 
                          followed by the plcpcd (piece table) for the document. */
                  lcbClx = getlong(WordDocument, 0x164);/* lcbClx - count of bytes of complex file information. == 0 if file is non-complex.*/
                  /* now we get the clx array - be carreful, ONLY for fast saved docs ! */  
                  clx = g_malloc0(lcbClx);
                  memcpy(clx, WordDocument+fcClx, lcbClx);
                  /* now we are looking for the Piecetable structures */
                  count =0;
                  goOn = TRUE;
                  while(goOn) {
                     typeEntry = clx[count];
                     if(typeEntry==2) {
                        goOn = FALSE;
                        lcbPieceTable = getshort(clx, count+1);
                        pieceTable = g_malloc0(lcbPieceTable);
                        memcpy(pieceTable, clx+count+5, lcbPieceTable);
                        pieceCount = (lcbPieceTable-4)/12;
                        /* here we must build the big-block in order to merge all pieces */
                        unsigned char *BBOT = NULL; /* Big Block Of Text */
                        BBOT = g_malloc0 (getlong(WordDocument, 52) ); /* we hope it's only ANSI chars ! */
                        outputFile = fopen(path_to_tmp_file, "w");
                        for(i=0;i<pieceCount;i++) {
                        /* text block position - text block descriptor */
                          offsetPieceDescriptor = ((pieceCount+1)*4)+(i*8);
                          memcpy(pieceDescriptor, pieceTable+offsetPieceDescriptor, 8);
                          fcValue = getlong(pieceDescriptor,2) & 0x40000000;                                                       
                          fcOffset = getlong(pieceDescriptor,2) & 0xBFFFFFFF;
                          textstart = getlong(pieceTable, i*4);
                          textlen = getlong(pieceTable, i*4+4);
                          /* warning, UNICODE bad detected !!! */
                          if(textlen<getlong(WordDocument, 52))
                                 memcpy(BBOT+textstart, (gchar *)(WordDocument)+fcOffset,textlen -textstart);
                                        
                        }/* next i */
                        //str = MSWordconvert_str((gchar *)BBOT, "WINDOWS-1252", "UTF8", getlong(WordDocument, 52) );
                        str = MSWordconvert_str((gchar *)BBOT, iCpW1252, getlong(WordDocument, 52) );
                        fwrite(str,sizeof(gchar), strlen(str), outputFile);                    
                        g_free(str);   
 
                        fwrite(end_sign, sizeof(gchar),strlen(end_sign), outputFile); 

                        fclose(outputFile);
                        g_free(pieceTable);
                        g_free(BBOT);
                     }/* typentry =2*/
                     else if(typeEntry==1) {
                            count = count+1+1+clx[count+1]+1;
                          }
                     else if(typeEntry==0) {
                            count=count+2;
                          }
                     else {
                       goOn = FALSE;
                     }/* other entry */
                  }/* wend goOn */ 
                  g_free(clx);
            }/* fast saved */    
           else {
                   outputFile = fopen(path_to_tmp_file, "w");
                   str = MSWordconvert_str((gchar *)WordDocument+textstart, iCpW1252, textlen );
                   //str=  g_convert_with_fallback ((gchar *)(WordDocument)+textstart, textlen, 
                     // "UTF8", "WINDOWS-1252", NULL, &bytes_read, &bytes_written, &error);
                   fwrite(str,sizeof(gchar), strlen(str), outputFile);
                   g_free(str);    
                   fwrite(end_sign, sizeof(gchar),strlen(end_sign), outputFile);            
                   fclose(outputFile);
           }/* else not fast saved */        
           /* we clean the memory */
           g_free(directoryChain);
           g_free(WordDocument);
           g_free(SAT);  
           g_free(SSAT);
           g_free(SSCS);
           g_free(buffer);
           return path_to_tmp_file; 
  }/* word 6-95 */

  /* here we continue with W97+ files */
  /* we read the csw value just after the fubbase structure - MUST BE 0x000E */
  fileOffset = 32;/* fibBase has a length of 32 bytes */
  /* thus we can skip the fibRgW structure - 28 bytes +2 */
  fileOffset = fileOffset+30;
  /* thus we can skip the fibRgLw structure = 88 bytes+2 */
  fileOffset = fileOffset+90;
  /* we read the cbRgFcLcb - it has various pre-defined values */
  fileOffset = fileOffset+2;
  /* we have an offset on the top of the FibRgFcLcb97 structure (744 bytes) now we are looking for 
     FibRgFcLcb97.fcClx which is an offset on the table  fcClx is the 67th 64 bits value and lcbClx the 68th*/
  /* here, fileOffset is at byte 154, e.g. the start of FibRgFcLcb97 structure */
  fcClx = getlong(WordDocument,fileOffset+(4*66));
  lcbClx = getlong(WordDocument,fileOffset+(4*67));
  /* now we build the xx Table stream in memory */
  if( fTableStream>=0) {
          count = 0;
          tableStream = NULL; 
          if(TableStreamLen<minSizeSectorStream) {
                while(sectIdTableStream!=-2) {
                  tableStream = g_realloc(tableStream, (count+1)*shortSectorSize);
                  memcpy(tableStream+count*shortSectorSize, SSCS+(sectIdTableStream*shortSectorSize), shortSectorSize);
                  sectIdTableStream = getlong(SSAT, sectIdTableStream*4);
                  count++;
                }/* wend */
          }
          else{
                while(sectIdTableStream!=-2) {
                  tableStream = g_realloc(tableStream, (count+1)*sectorSize);
                  memcpy(tableStream+count*sectorSize, buffer+512+(sectIdTableStream*sectorSize), sectorSize);
                  sectIdTableStream = getlong(SAT, sectIdTableStream*4);
                  count++;
                }/* wend */ 
          }/* elseif SAT */
         /* now we get the clx array */  
         clx = g_malloc0(lcbClx);
         memcpy(clx, tableStream+fcClx, lcbClx);
        /* now we are looking for the Piecetable structures */
         count =0;
         goOn = TRUE;
         while(goOn) {
           typeEntry = clx[count];
           if(typeEntry==2) {
             goOn = FALSE;
             lcbPieceTable = getlong(clx, count+1);
             pieceTable = g_malloc0(lcbPieceTable);
             memcpy(pieceTable, clx+count+5, lcbPieceTable);
             pieceCount = (lcbPieceTable-4)/12;
             outputFile = fopen(path_to_tmp_file, "w");
             for(i=0;i<pieceCount;i++) {
               /* text block position ,  text block descriptor */
               offsetPieceDescriptor = ((pieceCount+1)*4)+(i*8);
               memcpy(pieceDescriptor, pieceTable+offsetPieceDescriptor, 8);
               fcValue = getlong(pieceDescriptor, 2) & 0x40000000;                                                       
               fcOffset = getlong(pieceDescriptor, 2) & 0xBFFFFFFF;
               textstart = getlong(pieceTable, i*4);
               textlen = getlong(pieceTable, i*4+4);
               if((fcValue==0x40000000)==FALSE) {/* char codage : Unicode 2 bytes by char */
                 // str = MSWordconvert_str((gchar *)(WordDocument)+fcOffset, "UTF16", "UTF8", 2*(textlen -textstart ));
                 str = MSWordconvert_str((gchar *)(WordDocument)+fcOffset, iCpUtf16, 2*(textlen -textstart ));
                 //str=  g_convert_with_fallback ((gchar *)(WordDocument)+fcOffset, 2*(textlen -textstart), 
                   //   "UTF8", "UTF16", NULL, &bytes_read, &bytes_written, &error);
                 fwrite(str,sizeof(gchar), strlen(str), outputFile);
                 g_free(str);
               }
               else {/* chars codage : WINDOWS-1252 1 byte by char */
                 str = MSWordconvert_str((gchar *)(WordDocument)+fcOffset/2, iCpW1252, textlen -textstart );
               //  str=  g_convert_with_fallback ((gchar *)(WordDocument)+fcOffset/2, textlen-textstart, 
                 //     "UTF8", "WINDOWS-1252", NULL, &bytes_read, &bytes_written, &error);
                 fwrite(str,sizeof(gchar), strlen(str), outputFile);
                 g_free(str);
               }
             }/* next */
             fwrite(end_sign, sizeof(gchar),strlen(end_sign), outputFile); 
             fclose(outputFile);
           }
           else if(typeEntry==1) {
             count = count+1+1+clx[count+1];
           }
           else {
             goOn = FALSE;
           }
         }/* wend */  
         g_free(clx);
         g_free(tableStream); 
         g_free(pieceTable);
       }/* if fTableStream */
  /* we clean the memory */
  g_free(directoryChain);
  g_free(WordDocument);
  g_free(SAT);  
  g_free(SSAT);
  g_free(SSCS);
  g_free(buffer);
  return path_to_tmp_file;
}

/*********************************************
   ms-RTF parser
*********************************************/
gchar *RTFCheckFile(gchar *path_to_file, gchar *path_to_tmp_file)
{
  FILE *outputFile, *inputFile;
  gint  i, j, fileSize = 0, openedBraces = 0;
  gchar *buffer = NULL;
  gboolean fParagraph = FALSE;
  GError **error;
  glong bytes_written, bytes_read;
  gint hexa_char;
  gchar buf_hexa[32];
  GString *str=g_string_new("");

  inputFile = fopen(path_to_file,"rb");
  if(inputFile==NULL) {
          printf("* ERROR : can't open RTF file:%s *\n", path_to_file);
          return NULL;
  }
  /* we compute the size before dynamically allocate buffer */
  glong prev = ftell(inputFile);   
  fseek(inputFile, 0L, SEEK_END);
  glong sz = ftell(inputFile);
  fseek(inputFile, prev, SEEK_SET);

  /* we allocate the buffer */
  buffer = g_malloc0(sz*sizeof(gchar)+sizeof(gchar));
  /* we start the file reading in Binary mode : it's better for future parsing */
  fileSize = fread(buffer, sizeof(gchar), sz, inputFile);
  fclose(inputFile);
  i=0;
  /* we try to open the output file */
   outputFile = fopen(path_to_tmp_file, "w"); 
   if(outputFile==NULL)
     return NULL;
   while(i<=fileSize) {
    /* is it a command char ? */
    switch(buffer[i])
     {
       case '\\':{
         i++;/* next char */        
         switch(buffer[i])
          {
            case '*':{openedBraces = 1;
               while( (i<fileSize)&&(openedBraces>0)){
                /* we skip all chars */
                i++;
                        if(buffer[i]=='}')
                           openedBraces--;
                        if(buffer[i]=='{')
                           openedBraces++;
               }
              i--;
              break;           
            }
            case '~':{
              str= g_string_append(str, " ");
              break;
            }
            case '\\':{
              str= g_string_append (str, "\\");
              break;
            }
            case '{':{
              str= g_string_append (str, "{");
              break;
            }
            case '}':{
              str= g_string_append (str, "}");
              break;
            }
            case '-': case '_':{
              str= g_string_append (str, "-");
              break;
            }
            case '\'':{/* char >127 in Hexa mode */
                          buf_hexa[0] = buffer[i+1];
                          buf_hexa[1] = buffer[i+2];
                          buf_hexa[2]=0;
                          buf_hexa[3]=0;
                          sscanf(buf_hexa, "%x", &hexa_char);
                          buf_hexa[0] = hexa_char;
                          buf_hexa[1]=0;   
                          str= g_string_append (str,  g_convert_with_fallback ((gchar *)buf_hexa, 1, "UTF8", "WINDOWS-1252",
                                           NULL, &bytes_read, &bytes_written, &error));                    
                          i=i+2;

              break;
            }
            case 'u':{/* char >127 in decimal mode */
                         i++;
                         j=0;
                         while((i<fileSize) && ( (buffer[i]>='0') && (buffer[i]<='9')))
                           {
                             buf_hexa[j]= buffer[i];
                             i++;
                             j++;
                         }/* wend digits in U chars */
                         if(j>0) {
                           buf_hexa[j]=0;
                           sscanf(buf_hexa, "%d", &hexa_char);
                           if(hexa_char>255) {
                               buf_hexa[0]= hexa_char-(256*(hexa_char/256));
                               buf_hexa[1]= hexa_char/256;
                               buf_hexa[2]=0;
                               buf_hexa[3]=0;
                               buf_hexa[4]=0;
                               if(j>0) {
                                  str= g_string_append (str, g_convert_with_fallback ((gchar *)buf_hexa, 2, "UTF8", "UTF16",
                                           NULL, &bytes_read, &bytes_written, &error));
                               }

                           }/* >255 */
                           else {
                             buf_hexa[0] = hexa_char;/* be careful for chars >255 !!! */
                             buf_hexa[1]=0;
                             if(j>0) {
                                str= g_string_append (str, g_convert_with_fallback ((gchar *)buf_hexa, 1, "UTF8", "WINDOWS-1252",
                                           NULL, &bytes_read, &bytes_written, &error));
                             }
                           }/* <256 */
                           /* we now check if there is an alternate coding */
                           if(i<fileSize-2) {
                             if((buffer[i]=='\\') && (buffer[i+1]=='\'')){
                              i=i+4;/* warning works only for 8 bits Hex values  */
                             }
                           }
                         }
                         else {
                            while( (i<fileSize)&&(buffer[i]!=' ')&&(buffer[i]!='\\')&&(buffer[i]!='}')&&(buffer[i]!='{')) {
                               i++;
                            }
                         }      
              i--;
              break;
            }
            default:{/* ? a true control ? */
              /* at least, we must check if it's a paragraph command */
              if( g_ascii_strncasecmp ((gchar*)&buffer[i],"pard",4*sizeof(gchar))==0) {
                      i=i+4;
                      fParagraph = TRUE;
              }
              else
                if(g_ascii_strncasecmp ((gchar*)&buffer[i],"par",3*sizeof(gchar))==0) {
                      i=i+3;
                      fParagraph = FALSE;
                      str= g_string_append (str, "\n");
                }
                /* we must add the case of \pict controls ! */
                else
                  if(g_ascii_strncasecmp ((gchar*)&buffer[i],"pict",4*sizeof(gchar))==0){
                     /* 2 cases ; simple picture, then no braces in other cases we have \* and braces */
                     i=i+4;
                     openedBraces = 1;
                     while((i<fileSize)&&(openedBraces>=0)) {
                        /* we skip all picture's hexa codes */
                        i++;
                        if(buffer[i]=='}')
                           openedBraces--;
                        if(buffer[i]=='{')
                           openedBraces++;
                     }/* wend */
                   }
                   else {
                        while((i<fileSize)&&(buffer[i]!=' ')&&(buffer[i]!='\\')&&(buffer[i]!='}')&&(buffer[i]!='{') &&(buffer[i]!='\n')) {
                        /* we skip all chars */
                        i++;
                        }/* wend */
                   }/* elseif */
             i--;
            }/* case default */
          }/* end switch first char after control */
         break;
       }
       case '{':
       case '}':
       case '\n':{/* a Parser MUST ignore LF */
         break;
       }      
       default:{
        if(fParagraph) {
            str= g_string_append_c (str, buffer[i]);
        }
       }
     }/* end switch */
    i++;
   }/* wend all along the file */   
 g_free(buffer);
 fwrite(str->str,sizeof(gchar), strlen(str->str), outputFile);
 misc_close_file(outputFile);
 g_string_free(str, TRUE);
 return path_to_tmp_file;
}

/***********************************************
 OLD WinWord files
**********************************************/
gchar *WRDCheckFile(gchar *path_to_file, gchar *path_to_tmp_file)
{
  FILE *outputFile, *inputFile;
  gint flags,charset;
  glong textstart,textlen, fileSize, i=0, j=0;
  gchar *buffer, c, *str=NULL, buf_hexa[32];
  GError **error = NULL;
  glong bytes_written, bytes_read;

  /* the Winword file is binary */
  inputFile = fopen(path_to_file,"rb");
  if(inputFile==NULL) {
          printf("* ERROR : impossible to open MS Winword file:%s *\n", path_to_file);
          return NULL;
  }
  /* we compute the size before dynamically allocate buffer */
  glong prev = ftell(inputFile);   
  fseek(inputFile, 0L, SEEK_END);
  glong sz = ftell(inputFile);
  fseek(inputFile, prev, SEEK_SET);
  /* we allocate the buffer */
  buffer = g_malloc0(sz*sizeof(gchar)+sizeof(gchar));
  /* we start the file reading in Binary mode : it's better for future parsing */
  fileSize = fread(buffer, sizeof(gchar), sz, inputFile);
  fclose(inputFile);
  /* various analysis */
  flags = getshort(buffer,10);
  if (flags & fExtChar) {
			printf ("File uses extended character set\n");

	} else 
		if (buffer[18]) {
			printf("File created on Macintosh\n");
		} else {
			printf("File created on Windows\n");
		} 

  if (flags & fEncrypted) {
		fprintf(stderr,"[File is encrypted. Encryption key = %08lx]\n",
				getlong(buffer,14));
		return -1;
	}

  charset=getshort(buffer,20);
  if (charset&&charset !=256) {
			printf("Using character set %d\n",charset);
		} else {
			printf("Using default character set\n");
		}
  /* skipping to textstart and computing textend */
  textstart=getlong(buffer,24);
  textlen=getlong(buffer,28)-textstart-20;
  if((textstart<0) || (textlen<0) || (textlen<textstart)) {
    g_free(buffer);
    return NULL;
  }
  /* now we read and convert datas */
  i = textstart;
  outputFile = fopen(path_to_tmp_file, "w"); 
  if(outputFile==NULL) {
    g_free(buffer);
    return NULL;
  }

 // str = MSWordconvert_str((gchar *)buffer+i, iCpIso8859_1, textlen );
 // fwrite(str, sizeof(gchar), strlen(str), outputFile);
  //fwrite("\0", sizeof(gchar), 1, outputFile);
  while((i<fileSize) && (j<textlen-1)) {
    c = buffer[i];     
    switch(c) {
      case 0: case 0x0D: {j++;break;}
      case '\n': {
        fwrite((gchar*)"\n",sizeof(gchar), 1, outputFile);
        j++;
        break;
      }
      default:{
        if(((gint)c<128) && (c>31)){
          fwrite(&buffer[i], sizeof(gchar), sizeof(gchar), outputFile);
          j++;
        }
        else {
         if((guint)(c)>127) {                
            buf_hexa[0] = (guint)c;
            buf_hexa[1] = 0;
            str= g_convert_with_fallback ((gchar *)buf_hexa, -1, "UTF8", "ISO-8859-1",
                                           NULL, &bytes_read, &bytes_written, &error);
            fwrite(str, sizeof(gchar), strlen(str), outputFile);
            g_free(str);
            j++;
         }
        }
      }/* default */
    }/* end switch */    
    i++;
  }/* wend */
  misc_close_file(outputFile);
  return path_to_tmp_file;
}
/*********************************************
 Old Ms Write for Windows files
 the issue is that they uses two code pages :
 CP437
 CP850
 I haven't doc, so I've found that One Byte
 seems to indicates CP850
*********************************************/
gchar *WRICheckFile(gchar *path_to_file, gchar *path_to_tmp_file)
{
  FILE *outputFile, *inputFile;
  gchar *buffer = NULL, *str=NULL;
  gboolean fIsCP437 = FALSE;
  GError **error;
  glong msWordLength, fileSize;

  /* the WRITE file is binary */
  inputFile = fopen(path_to_file,"rb");
  if(inputFile==NULL) {
          printf("* ERROR : impossible to open MS Write file:%s *\n", path_to_file);
          return NULL;
  }
  /* we compute the size before dynamically allocate buffer */
  glong prev = ftell(inputFile);   
  fseek(inputFile, 0L, SEEK_END);
  glong sz = ftell(inputFile);
  fseek(inputFile, prev, SEEK_SET);
  /* we allocate the buffer */
  buffer = g_malloc0(sz*sizeof(gchar)+sizeof(gchar));
  /* we start the file reading in Binary mode : it's better for future parsing */
  fileSize = fread(buffer, sizeof(gchar), sz, inputFile);
  fclose(inputFile);
  /* we save the MsWrite content - very basic, I haven't documentation */
  outputFile = fopen(path_to_tmp_file, "w"); 
  if(outputFile==NULL) {
        g_free(buffer);
        return NULL;
  }
  /* it seems that the end of text is coded in bytes[14-15] - Luc A 17 march 2018 */
  msWordLength = getshort(buffer,14)-128;

  if(buffer[20]==0x14)/* it's an hypothesis - Luc A 17 march 2018 */
        fIsCP437 =TRUE;
  if(msWordLength<0) {
      g_free(buffer);
      return NULL;
  }

  if(fIsCP437)
        str = MSWordconvert_str((gchar *)buffer+128, iCpIbm437, getshort(buffer,14)-128 );
        //str = g_convert_with_fallback ((gchar *)buffer+128, getshort(buffer,14)-128, "UTF8", "CP437",
          //                                 NULL, &bytes_read, &bytes_written, &error);
  else
        str = MSWordconvert_str((gchar *)buffer+128, iCpW1252, getshort(buffer,14)-128 );
       // str = g_convert_with_fallback ((gchar *)buffer+128, getshort(buffer,14)-128, "UTF8", "WINDOWS-1252",
         //                                  NULL, &bytes_read, &bytes_written, &error);
  fwrite(str , sizeof(gchar), strlen(str), outputFile);
  misc_close_file(outputFile);
  g_free(str);
  g_free(buffer);
  return path_to_tmp_file;
}
/*********************************************
  Abiword ABW non-compress text files

  With Abiword, it's simple to parse the XML 
  file :
<p  indicates the starting of a paragraph
 end </p> the end of the paragraph
*********************************************/
gchar *ABWCheckFile(gchar *path_to_file, gchar *path_to_tmp_file)
{
   gboolean fIsParagraph = FALSE;
   FILE *outputFile, *inputFile;
   gchar *pBufferRead, *str, c, *tmpfileToExtract = NULL, *buffer;
   glong  i, fileSize = 0, j=1; 
   
   /* the ABW file isn't compressed, so it can be opened as a basic plain text file */
   inputFile = fopen(path_to_file,"rb");
   if(inputFile==NULL) {
          printf("* ERROR : impossible to open ABW file:%s *\n", path_to_file);
          return NULL;
   }

   /* we compute the size before dynamically allocate buffer */
   glong prev = ftell(inputFile);   
   fseek(inputFile, 0L, SEEK_END);
   glong sz = ftell(inputFile);
   fseek(inputFile, prev, SEEK_SET);

   /* we allocate the buffer */
   buffer = g_malloc0(sz*sizeof(gchar)+sizeof(gchar));
   /* we start the file reading in Binary mode : it's better for future parsing */
   fileSize = fread(buffer, sizeof(gchar), sz, inputFile);
   fclose(inputFile);

   /* now, the parsing itself */
   outputFile = fopen(path_to_tmp_file,"w");
   if(outputFile==NULL) {
           g_free(buffer);           
           printf("* ERROR : impossible to open temporary file:%s in order to parse ABW*\n", path_to_tmp_file);
           return NULL;
   }
   /* first memory allocation */
   str = (gchar*)g_malloc(sizeof(gchar));
   str[0] = '\0';
   i=0;
   while(i<fileSize)
        {
         if(g_ascii_strncasecmp ((gchar*)  &buffer[i],"<", sizeof(gchar))  == 0)
            {
              do
               {
                 /* is it an end of paragraph ? If yes, we add a \n char to the buffer */
                 if(g_ascii_strncasecmp ((gchar*)  &buffer[i],"</p>",4*sizeof(gchar))  ==  0 ) {
                     str = (gchar*)realloc(str, j * sizeof(gchar));
                     str[j-1] = '\n';
                     j++;
                     i = i+4*sizeof(gchar); 
                     fIsParagraph = FALSE;
                 }
                 else{
                      /* is it an XML section for another thing that a paragraph ? */
                       if(g_ascii_strncasecmp ((gchar*)  &buffer[i],"<p",2*sizeof(gchar))  ==  0 ) {
                             i = i+2*sizeof(gchar); 
                             fIsParagraph = TRUE;                    
                       }
                    else{
                        if(g_ascii_strncasecmp ((gchar*)  &buffer[i],"</foot",6*sizeof(gchar))  ==  0 ) {/* yes, it's a strange way but it works ! after a footnote's end we start a paragraph*/
                             i = i+6*sizeof(gchar); 
                             fIsParagraph = TRUE;                    
                        }
                       }/* elseif footnotes */
                          
                 }/* else paragraph end */
                i= i+sizeof(gchar);     
               }
              while ( g_ascii_strncasecmp ((gchar*)  &buffer[i],">", sizeof(gchar))  !=0);
              i= i+sizeof(gchar); 
            }/* if */
          else 
           {
             /* is it a special string ? */
              if(g_ascii_strncasecmp ((gchar*)  &buffer[i],"&amp;",5*sizeof(gchar))  ==  0 ) { 
                   str = (gchar*)realloc(str, j * sizeof(gchar));
                   str[j-1] = '&';
                   j++;
                   i = i+5*sizeof(gchar); 
              }
              else
                 if(g_ascii_strncasecmp ((gchar*)  &buffer[i],"&apos;",6* sizeof(gchar))  ==  0 ) {
                     str = (gchar*)realloc(str, j * sizeof(gchar));
                     str[j-1] = '\'';
                     j++;
                     i = i+6*sizeof(gchar); 
                 }
                 else
                    if(g_ascii_strncasecmp ((gchar*)  &buffer[i],"&quot;",6* sizeof(gchar))  ==  0 ) {
                       str = (gchar*)realloc(str, j * sizeof(gchar));
                       str[j-1] = '"';
                       j++;
                       i = i+6*sizeof(gchar); 
                    }
                   else
                      if(g_ascii_strncasecmp ((gchar*)  &buffer[i],"&lt;",4* sizeof(gchar))  ==  0 ) {
                          str = (gchar*)realloc(str, j * sizeof(gchar));
                          str[j-1] = '<';
                          j++;
                          i = i+4*sizeof(gchar); 
                      }
                      else
                        if(g_ascii_strncasecmp ((gchar*)  &buffer[i],"&gt;",4* sizeof(gchar))  ==  0 ) {
                             str = (gchar*)realloc(str, j * sizeof(gchar));
                             str[j-1] = '>';
                             j++;
                             i = i+4*sizeof(gchar); 
                        }
                        else {
                           c = (gchar )buffer[i];            
                           if((c!='\n') && ( fIsParagraph)){
                             str = (gchar*)realloc(str, j * sizeof(gchar));
                             str[j-1] = c; 
                             //i= i+sizeof(gchar);
                           j++; 
                        }/* else */
                  i= i+sizeof(gchar);
               }
            // i= i+sizeof(gchar);              
         }/* else */           
      }/* wend i*/
   /* we have to add the nULL char at end of str */
   str[j-2]='\0';
   g_free(buffer);

   /* we dump the parsed file */
   //printf("longueur chaine :%d\n", strlen(str));
   fwrite(str , sizeof(gchar), strlen(str), outputFile);
   misc_close_file(outputFile);
   tmpfileToExtract = g_strdup_printf("%s", path_to_tmp_file);
   if(strlen(str)>0) 
          g_free(str);     
   return(tmpfileToExtract);
}

/*****************************
 function to obtain infos 
 about a PDF file
 We us many poppler funcs
 to obtain the infos at the
 same time
 the program passes the file,
 wich will be converted to
 URI format
****************************/
gchar *PDFCheckFile(gchar *path_to_file, gchar *path_to_tmp_file)
{
    GError* err = NULL;
    gchar *uri_path, *tmpfileToExtract = NULL;
    gchar *text_of_page = NULL;
    PopplerDocument *doc;
    PopplerPage *page;
    gint i, j, pdf_npages; /* integer used to store total amount of pages of the pdf file */
    FILE *outputFile;    

  /* first step : converting from path format to URI format */
    uri_path = g_filename_to_uri(path_to_file, NULL,NULL);

    doc = poppler_document_new_from_file(uri_path, NULL, &err);
    if (!doc) {
        printf("%s\n", err->message);
        g_error_free(err);
        return NULL;
    }

    pdf_npages = poppler_document_get_n_pages(doc);

    page = poppler_document_get_page(doc, 0);/* #0 = first page */
    if(!page) {
         printf("* Could not open first page of document *\n");
         g_object_unref(doc);
         return NULL;
    }
    outputFile = fopen(path_to_tmp_file,"w");

  /* display text from pages */
   for(i=0;i<pdf_npages;i++) {
    page = poppler_document_get_page(doc, i);
    text_of_page = poppler_page_get_text (page);

    for(j=0; j<strlen(text_of_page);j++)
      fputc((unsigned char)text_of_page[j], outputFile);
    g_free(text_of_page);
    fputc('\n', outputFile);
    g_object_unref(page);
   }/* next i*/
 misc_close_file(outputFile);
 tmpfileToExtract = g_strdup_printf("%s", path_to_tmp_file);

 return tmpfileToExtract;
}


/* OPenDocument Text ODT files : read a supposed docx file, unzip it, check if contains the stuff for a ODT writer file 
 * entry1 = path to the supposed ODT file
 * entry 2 = path to a filename, text coded located in system TEMPDIR
* return ; path to the content or NULL if not a true Doc-x file
*  NOTE : chars inside the doc-x are already coded in UTF-8 or extended ASCII with accented chars

* please NOTE : the text must be escaped for those chars : https://stackoverflow.com/questions/1091945/what-characters-do-i-need-to-escape-in-xml-documents

"   &quot;
'   &apos;
<   &lt;
>   &gt;
&   &amp

for example, to keep " in the resulting text, we must search and replace every &quot; found
NOTE : this function can also parse ODP and ODS files, but I've noticed that, in ODP files, some chars aren't in the same
phrase so the user must search for part of words in ODP files
 ***/
gchar *ODTCheckFile(gchar *path_to_file, gchar *path_to_tmp_file)
{
 gint checkFile = 0;
 gint err, i;
 gint nto=0; /* number of files in zip archive */
 gint exist_document_xml = -1;
 gchar buf[1024], *pBufferRead, *tmpfileToExtract = NULL;
 FILE *outputFile;
 struct zip *archive;
 struct zip_file *fpz=NULL;
 struct zip_stat sbzip;
 gchar *str, c;/* for dynamic string allocation */
 gint j=1; 
 /* opens the doc-x file supposed to be zipped */
  err=0;
  archive=zip_open(path_to_file,ZIP_CHECKCONS,&err);
  if(err != 0 || !archive)
        {
          zip_error_to_str(buf, sizeof(buf), err, errno);
          fprintf(stderr,"«%s» %d in function «%s» ,err=%d\n«%s»\n",__FILE__,__LINE__,__FUNCTION__,err,buf);
          return NULL;
        }
 
  nto = zip_get_num_entries(archive, ZIP_FL_UNCHANGED); 

  exist_document_xml = zip_name_locate(archive, "content.xml" ,0);
  if(exist_document_xml>-1)
     printf("* XML ODF document present in position:%d *\n", exist_document_xml);
  else {  
     printf("* Error ! %s isn't an ODF file ! *\n", path_to_file);
     return NULL;
  }
  /* now we must open document.xml with this index number*/
  fpz = zip_fopen_index(archive, exist_document_xml , 0); 
  assert (fpz!=NULL);/* exit if somewhat wrong is happend */

  /* we must know the size in bytes of document.xml */
   if(zip_stat_index( archive, exist_document_xml, 0, &sbzip) == -1) {
              strcpy(buf,zip_strerror(archive));
              fprintf(stderr,"«%s» %d in function «%s»\n«%s»\n",__FILE__,__LINE__,__FUNCTION__,buf);
              zip_fclose(fpz);
              zip_close(archive);
              return NULL;
   }

  /* copy document.xml to system TEMPDIR */
  pBufferRead = g_malloc(sbzip.size*sizeof(gchar)+sizeof(gchar)); 
  assert(pBufferRead != NULL);
  /* read all datas in buffer p and extract the document.xml file from doc-x, in memory */
  if(zip_fread(fpz, pBufferRead , sbzip.size) != sbzip.size) {
             fprintf(stderr,"«%s» %d in function «%s»\n«read error ...»\n",__FILE__,__LINE__,__FUNCTION__);
             g_free(pBufferRead);
             zip_fclose(fpz);
             zip_close(archive);
             return NULL;
  }/* erreur */
  
 /* in the TEMPDIR directory ! */
  outputFile = fopen(path_to_tmp_file,"w");
 
 /* parse and convert to pure text the document.xml */

 str = (gchar*)g_malloc(sizeof(gchar));
 
 i=0;
 while(i<sbzip.size)
   {
    if(g_ascii_strncasecmp ((gchar*)  &pBufferRead[i],"<", sizeof(gchar))  == 0)
        {
           do
            {
             /* is it an end of paragraph ? If yes, we add a \n char to the buffer */
             if(g_ascii_strncasecmp ((gchar*)  &pBufferRead[i],"</text:p>",9* sizeof(gchar))  ==  0 )
                {
                  str = (gchar*)realloc(str, j * sizeof(gchar));
                  str[j-1] = '\n';
                  j++;
                  i = i+9*sizeof(gchar); 
                }
             else
                i = i+sizeof(gchar);             
            }
           while ( g_ascii_strncasecmp ((gchar*)  &pBufferRead[i],">", sizeof(gchar))  !=0);
         i= i+sizeof(gchar); 
        }/* if */
       else 
           /* we have now to escape 5 special chars */
          {
              if(g_ascii_strncasecmp ((gchar*)  &pBufferRead[i],"&apos;",6* sizeof(gchar))  ==  0 )
                 {
                    str = (gchar*)realloc(str, j * sizeof(gchar));
                    str[j-1] = '\'';
                    j++;
                    i = i+6*sizeof(gchar); 
                 }
              else
                  if(g_ascii_strncasecmp ((gchar*)  &pBufferRead[i],"&quot;",6* sizeof(gchar))  ==  0 )
                    {
                       str = (gchar*)realloc(str, j * sizeof(gchar));
                       str[j-1] = '"';
                       j++;
                       i = i+6*sizeof(gchar); 
                    }
                   else
                     if(g_ascii_strncasecmp ((gchar*)  &pBufferRead[i],"&lt;",4* sizeof(gchar))  ==  0 )
                       {
                          str = (gchar*)realloc(str, j * sizeof(gchar));
                          str[j-1] = '<';
                          j++;
                          i = i+4*sizeof(gchar); 
                       }
                      else
                        if(g_ascii_strncasecmp ((gchar*)  &pBufferRead[i],"&gt;",4* sizeof(gchar))  ==  0 )
                          {
                             str = (gchar*)realloc(str, j * sizeof(gchar));
                             str[j-1] = '>';
                             j++;
                             i = i+4*sizeof(gchar); 
                         }
                        else
                          if(g_ascii_strncasecmp ((gchar*)  &pBufferRead[i],"&amp;",5* sizeof(gchar))  ==  0 )
                             {
                                str = (gchar*)realloc(str, j * sizeof(gchar));
                                str[j-1] = '&';
                                j++;
                                i = i+5*sizeof(gchar); 
                             }
                             else
                                {
                                  c = (gchar)pBufferRead[i]; // printf("%c", c);
                                  str = (gchar*)realloc(str, j * sizeof(gchar));
                                  str[j-1] = c; 
                                  j++;  // fwrite(&c , sizeof(gchar), 1, outputFile);
                                  i= i+sizeof(gchar);              
                                }
          }/* else */
   }/* wend */

  str = (gchar*)realloc(str, j * sizeof(gchar));
  str[j-1] = '\0'; /* at the end append null character to mark end of string mandatory with ODP files */

  fwrite(str , sizeof(gchar), strlen(str), outputFile);
  misc_close_file(outputFile);

 /* close the parsed file and clean datas*/
  tmpfileToExtract = g_strdup_printf("%s", path_to_tmp_file);

  if(str!=NULL) {    
        g_free(str);
        str = NULL;
  }
  if (pBufferRead != NULL)
     g_free (pBufferRead) ; /* discharge the datas of document.xml from memory */
  zip_fclose(fpz);/* close access to file in archive */
  zip_close(archive); /* close the doc-x file itself */
  return tmpfileToExtract;
}


/* Doc-X files : read a supposed docx file, unzip it, check if contains the stuff for a Doc-X word file 
 * entry1 = path to the supposed doc-x file
 * entry 2 = path to a filename, text coded located in system TEMPDIR
* return ; path to the Word content or NULL if not a true Doc-x file
*  NOTE : chars inside the doc-x are already coded in UTF-8 or extended ASCII with accented chars
 ***/
gchar *DocXCheckFile(gchar *path_to_file, gchar *path_to_tmp_file)
{
 gint checkFile = 0, err, i;
 gint nto=0; /* number of files in zip archive */
 gint exist_document_xml = -1;
 gchar buf[1024], *pBufferRead = NULL;
 gchar *tmpfileToExtract = NULL;
 FILE *outputFile;
 struct zip *archive;
 struct zip_file *fpz=NULL;
 struct zip_stat sbzip;
 gchar *str, c;/* for dynamic string allocation */
 glong j=1; /* security, because raw file content may be longer in bytes than $FFFF (if I apply my old knowledge from 8 bits ;-) */

 /* opens the doc-x file supposed to be zipped */
  err=0;https://fr.wikibooks.org/wiki/Programmation_C/Types_de_base
  archive=zip_open(path_to_file,ZIP_CHECKCONS,&err);
  if(err != 0 || !archive) {
          zip_error_to_str(buf, sizeof(buf), err, errno);
          fprintf(stderr,"«%s» %d in function «%s» ,err=%d\n«%s»\n",__FILE__,__LINE__,__FUNCTION__,err,buf);
          return NULL;
  }
 
  nto = zip_get_num_entries(archive, ZIP_FL_UNCHANGED); 
  
  /* checking if this is really a Word archive : in this cas, the doc-x mus contain a subdirectory /word containg document.xml file  like this "word/document.xml"*/
  exist_document_xml = zip_name_locate(archive, "word/document.xml" ,0);
  if(exist_document_xml>-1)
     printf("* XML Word document present in position:%d *\n", exist_document_xml);
  else {  
     printf("* Error ! %s isn't a Doc-X file ! *\n", path_to_file);
     return NULL;
  }
  /* now we must open document.xml with this index number*/
  fpz = zip_fopen_index(archive, exist_document_xml , 0); 
  assert (fpz!=NULL);/* exit if somewhat wrong is happend */

  /* we must know the size in bytes of document.xml */
   if(zip_stat_index( archive, exist_document_xml, 0, &sbzip) == -1)
           {
              strcpy(buf,zip_strerror(archive));
              fprintf(stderr,"«%s» %d in function «%s»\n«%s»\n",__FILE__,__LINE__,__FUNCTION__,buf);
              zip_fclose(fpz);
              zip_close(archive);
              return NULL;
           }

  /* copy document.xml to system TEMPDIR */
  pBufferRead = g_malloc(sbzip.size*sizeof(gchar)+1); 
  assert(pBufferRead != NULL);
  /* read all datas in buffer p and extract the document.xml file from doc-x, in memory */
  if(zip_fread(fpz, pBufferRead , sbzip.size) != sbzip.size) {
             fprintf(stderr,"«%s» %d in function «%s»\n«read error ...»\n",__FILE__,__LINE__,__FUNCTION__);
             g_free(pBufferRead);
             zip_fclose(fpz);
             zip_close(archive);
             return NULL;
  }/* erreur */

  outputFile = fopen(path_to_tmp_file,"w");
 
 /* parse and convert to pure text the document.xml */

 str = (gchar*)g_malloc(sizeof(gchar));

 i=0;
 while(i<sbzip.size)
   {
    if(g_ascii_strncasecmp ((gchar*)  &pBufferRead[i],"<", sizeof(gchar))  == 0)
        {
           do
            {
             /* is it an end of paragraph ? If yes, we add a \n char to the buffer */
             if(g_ascii_strncasecmp ((gchar*)  &pBufferRead[i],"</w:t>",6* sizeof(gchar))  ==  0 )
                {
                  str = (gchar*)realloc(str, j * sizeof(gchar));
                  str[j-1] = '\n';
                  j++;
                  i = i+6*sizeof(gchar); 
                }
             else
                i = i+sizeof(gchar);             
            }
           while ( g_ascii_strncasecmp ((gchar*)  &pBufferRead[i],">", sizeof(gchar))  !=0);
         i= i+sizeof(gchar); 
        }/* if */
       else 
          {
              c = (gchar)pBufferRead[i]; // printf("%c", c);
              str = (gchar*)realloc(str, j * sizeof(gchar));
              str[j-1] = c; 
              j++;  
              i= i+sizeof(gchar);              
          }/* else */
   }/* wend */

 
  fwrite(str , sizeof(gchar), strlen(str), outputFile);

 /* close the parsed file and clean datas*/

  tmpfileToExtract = g_strdup_printf("%s", path_to_tmp_file);// pourquoi plantage ici ????
  misc_close_file(outputFile);

  if(str!=NULL) /* il y a un pb sur str dans certains cas ! */
     g_free(str);
  if(pBufferRead!=NULL)
     g_free (pBufferRead) ; /* discharge the datas of document.xml from memory */
  zip_fclose(fpz);/* close access to file in archive */
  zip_close(archive); /* close the doc-x file itself */
 return tmpfileToExtract;
}


/*
/* function to convert a gchar extension type file in gint to switch 
/* Luc A - 1 janv 2018 from URL = https://stackoverflow.com/questions/4014827/best-way-to-switch-on-a-string-in-c #22 post
// please update in search.h the #define MAX_FORMAT_LIST according to the size of this table - Luc A 1 janv 2018
*/
gint keyfromstring(gchar *key2)
{
 gint i, i_comp;

 for(i=0;i<MAX_FORMAT_LIST;i++) {
    i_comp = g_ascii_strncasecmp (lookuptable[i].key, key2,4);
    if(i_comp==0) {
       return i;
    }
 }
 return FORMAT_OTHERS;
}


/****************************************************
 function to get an icon from installation folder

entry : the 'pFileType' of the file, from newMatch->pFileType
We must remove the 5 last chars, which can be -type or ~type

Luc A. - Janv 2018
*****************************************************/
GdkPixbuf *get_icon_for_display(gchar *stype)
{
  GdkPixbuf *icon = NULL; /* Luc A - 1 janv 2018 */
  GError *error = NULL;
  gchar *str_lowcase = NULL, *str_icon_file = NULL;
  gint i;

  i = keyfromstring(stype);/* the g_ascii_strn() function is case independant */

  str_icon_file = g_strconcat(PACKAGE_DATA_DIR, "/pixmaps/", PACKAGE, "/", 
                               lookuptable[i].icon_file_name, NULL);
  icon = gdk_pixbuf_new_from_file(str_icon_file, &error);
  
  if (error) {
      g_warning ("Could not load icon: %s\n", error->message);
      g_error_free(error);
      error = NULL;
  }
  g_free(str_icon_file);
  return icon;
}


/*
 * Internal Helper: Converts the date/modified criteria into internal data format
widget = mainAppWindow !!!
 */
void getSearchExtras(GtkWidget *widget, searchControl *mSearchControl)
{
  gchar *tmpString, *endChar;
  gint i, multiplierLess, multiplierMore =0;
  gboolean fMoreThanCheck = FALSE, fLessThanCheck = FALSE;
  gboolean fBeforeCheck = FALSE, fAfterCheck = FALSE;
  gboolean fIntervalCheck = FALSE, fSinceCheck = FALSE;
  gboolean fTodayCheck = FALSE;
  gint date_mode = 0;
  gdouble tmpDouble, tmpMultiplierLess, tmpMultiplierMore, tmpMoreThan =0, tmpLessThan=0;
  GDate DateAfter, DateBefore;
  gchar buffer[MAX_FILENAME_STRING + 1];
  struct tm tptr;
  time_t rawtime; 
  GDate current_date, previous_date;
  gint current_since_unit = 0, current_since_value = 1;

  GKeyFile *keyString = getGKeyFile(GTK_WIDGET(lookup_widget(widget, "window1")));

  const gchar *after = gtk_button_get_label (GTK_BUTTON (lookup_widget(widget, "afterEntry") ) );
  const gchar *before = gtk_button_get_label (GTK_BUTTON (lookup_widget(widget, "beforeEntry") ) );
  const gchar *intervalStart = gtk_button_get_label (GTK_BUTTON (lookup_widget(widget, "intervalStartEntry") ) );
  const gchar *intervalEnd = gtk_button_get_label (GTK_BUTTON (lookup_widget(widget, "intervalEndEntry") ) );

  const gint tmpSinceEntry = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(lookup_widget(widget, "entrySince")));
  const gint tmpSinceUnits = gtk_combo_box_get_active( GTK_COMBO_BOX(lookup_widget(widget,"sinceUnits")));

  const gchar *moreThan = gtk_entry_get_text(GTK_ENTRY(lookup_widget(widget, "moreThanEntry")));
  const gchar *lessThan = gtk_entry_get_text(GTK_ENTRY(lookup_widget(widget, "lessThanEntry")));

  const gint tmpDepth = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(lookup_widget(widget, "folderDepthSpin")));
  const gint tmpLimit = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(lookup_widget(widget, "maxHitsSpinResults")));
  const gint tmpContentLimit = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(lookup_widget(widget, "maxContentHitsSpinResults")));

  const gint tmpLines = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(lookup_widget(widget, "showLinesSpinResults")));
  /* Luc A - janv 2018 */

  multiplierMore = gtk_combo_box_get_active( GTK_COMBO_BOX(lookup_widget(widget,"MoreThanSize")));
  multiplierLess = gtk_combo_box_get_active( GTK_COMBO_BOX(lookup_widget(widget,"LessThanSize")));

  gint kb_multiplier_more_than = multiplierMore;
  gint kb_multiplier_less_than = multiplierLess;

  /* get the flags from a string to a boolean ; set-up the gint for switches */

  if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(lookup_widget(widget, "lessThanCheck"))) )
     {fLessThanCheck = TRUE;}
  if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(lookup_widget(widget, "moreThanCheck"))))
     {fMoreThanCheck = TRUE;}
  if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(lookup_widget(widget, "todayCheck"))) )
    {fTodayCheck = TRUE;date_mode = 1;}
  if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(lookup_widget(widget, "afterCheck"))) )
    {fAfterCheck = TRUE;date_mode = 2;}
  if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(lookup_widget(widget, "beforeCheck"))))
    {fBeforeCheck = TRUE;date_mode = 3;}
  if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(lookup_widget(widget, "intervalCheck"))))
    {fIntervalCheck = TRUE;date_mode = 4;}
  if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(lookup_widget(widget, "sinceCheck"))))
    {fSinceCheck = TRUE;date_mode = 5;}
   
  /* get current size values */
  if(moreThan!=NULL )
     tmpMoreThan = strtod(moreThan, &endChar);
  if(lessThan!=NULL)
     tmpLessThan = strtod(lessThan, &endChar);

  /* convert the current size Unit to Kb */
  if(kb_multiplier_more_than<0)
      kb_multiplier_more_than = 0;
  if(kb_multiplier_less_than<0)
      kb_multiplier_less_than = 0;
  tmpMultiplierLess = 1;
  tmpMultiplierMore = 1;
  for(i=0;i<kb_multiplier_more_than;i++) {
       tmpMultiplierMore = tmpMultiplierMore*1024;
       tmpMoreThan = tmpMoreThan*1024;
  }
  for(i=0;i<kb_multiplier_less_than;i++) {
       tmpMultiplierLess = tmpMultiplierLess*1024;
       tmpLessThan = tmpLessThan*1024;
  }

  if (getExpertSearchMode(widget) == FALSE) {
    mSearchControl->numExtraLines = 0;
    mSearchControl->flags |= SEARCH_EXTRA_LINES;
    return;
  }
  /* Read extra lines spin box */
  mSearchControl->numExtraLines = tmpLines;

  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(lookup_widget(widget, "showLinesCheckResults")))) {
    mSearchControl->flags |= SEARCH_EXTRA_LINES;
  }

  /* Read result limit option */
  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(lookup_widget(widget, "limitContentsCheckResults")))) {
    mSearchControl->limitContentResults = tmpContentLimit;
    mSearchControl->flags |= SEARCH_LIMIT_CONTENT_SHOWN;
  }

  /* Read result limit option */
  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(lookup_widget(widget, "limitResultsCheckResults")))) {
    mSearchControl->limitResults = tmpLimit;
    mSearchControl->flags |= LIMIT_RESULTS_SET;
  }

 /* Read folder depth setting */
  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(lookup_widget(widget, "folderDepthCheck")))) {
    mSearchControl->folderDepth = tmpDepth;
    mSearchControl->flags |= DEPTH_RESTRICT_SET;
  }
  /* Read file min/max size strings */
  if ((fMoreThanCheck) && (moreThan != NULL)) {
      tmpDouble = strtod(moreThan, &endChar);
      if (tmpDouble <= 0) {
           miscErrorDialog(widget, _("<b>Error!</b>\n\nMoreThan file size must be <i>positive</i> value\nSo this criteria will not be used.\n\nPlease, check your entry and the units."));
          return;
      }
      if ((fLessThanCheck) && (tmpMoreThan >=tmpLessThan)) {
           miscErrorDialog(widget, _("<b>Error!</b>\n\nMoreThan file size must be <i>stricly inferior</i> to LessThan file size.So this criteria will not be used.\n\nPlease, check your entry and the units."));      
          return;
      }
      g_ascii_formatd (buffer, MAX_FILENAME_STRING, "%1.1f", tmpDouble);
      g_key_file_set_string (keyString, "history", "moreThanEntry",buffer);
      mSearchControl->moreThan = (gsize)(tmpMultiplierMore*1024 * tmpDouble);/* modif Luc A janv 2018 */
      mSearchControl->flags |= SEARCH_MORETHAN_SET;
  }
  if ((fLessThanCheck) &&  (lessThan != NULL)) {
      tmpDouble = strtod (lessThan, &endChar);
      if (tmpDouble <= 0) {
           miscErrorDialog(widget, _("<b>Error!</b>\n\nLessThan file size must be <i>positive</i> value\nSo this criteria will not be used.\n\nPlease, check your entry and the units."));
          return;
      }
      if ((fMoreThanCheck) && (tmpMoreThan >=tmpLessThan)) {
           miscErrorDialog(widget, _("<b>Error!</b>\n\nMoreThan file size must be <i>stricly inferior</i> to LessThan file size.So this criteria will not be used.\n\nPlease, check your entry and the units."));
          return;
      }
      g_ascii_formatd (buffer, MAX_FILENAME_STRING, "%1.1f", tmpDouble);
      g_key_file_set_string (keyString, "history", "lessThanEntry",buffer);
      mSearchControl->lessThan = (gsize)(tmpMultiplierLess*1024 * tmpDouble);
      mSearchControl->flags |= SEARCH_LESSTHAN_SET;
  }

  /* Read date strings */

  switch(date_mode )
    {
      case 1:{
        /* we get the current date */
        time ( &rawtime );
        strftime(buffer, 80, "%x", localtime(&rawtime));/* don't change parameter %x */
        g_date_set_parse(&current_date, buffer);
        g_date_strftime(buffer, 75, _("%x"), &current_date);
        setTimeFromDate(&tptr, &current_date);
        mSearchControl->after = mktime(&tptr); 
        mSearchControl->flags |= SEARCH_AFTER_SET;
        break;
      }
      case 2: {
         if ((fAfterCheck) && ((after != NULL) && (after != '\0'))) {
           g_date_set_parse(&DateAfter, after);
           if (!g_date_valid(&DateAfter)) {
             miscErrorDialog(widget,_("<b>Error!</b>\n\nInvalid 'After'date - format as dd/mm/yyyy or dd mmm yy."));
             return;
           }/* if datevalid */
           if (g_date_strftime(buffer, MAX_FILENAME_STRING, _("%x"), &DateAfter) > 0) {/* !!!!!! ddoit être revu */
             }
           setTimeFromDate(&tptr, &DateAfter);
           mSearchControl->after = mktime(&tptr); 
           mSearchControl->flags |= SEARCH_AFTER_SET;
         }
        break;
      }
      case 3: {
         if ((fBeforeCheck) && ((before != NULL) && (before != '\0'))) {
          g_date_set_parse(&DateBefore, before);
          if (!g_date_valid(&DateBefore)) {
             miscErrorDialog(widget, _("<b>Error!</b>\n\nInvalid 'Before' date - format as dd/mm/yyyy or dd mmm yy."));
             return;
          }
          if (g_date_strftime(buffer, MAX_FILENAME_STRING, _("%x"), &DateBefore) > 0) {/* !!!!!! ddoit être revu */
             }
          /* cool hack : before must be interpreted like this <=date, so, it's easy to add ONE day ;-) to do that */

          g_date_add_days (&DateBefore,1);
          g_date_strftime(buffer, MAX_FILENAME_STRING, _("%x"), &DateBefore);
          setTimeFromDate(&tptr, &DateBefore);
          mSearchControl->before = mktime(&tptr);
          mSearchControl->flags |= SEARCH_BEFORE_SET;
         }
        break;
      }
      case 4:{
        /* the dates are previously checked in Dialog box, so we can take our risks ;-) */
        g_date_set_parse(&DateBefore,intervalEnd );
        g_date_set_parse(&DateAfter,intervalStart );
        if (!g_date_valid(&DateAfter) || !g_date_valid(&DateBefore)) {
             miscErrorDialog(widget,_("<b>Error!</b>\n\nInvalid 'Interval'date(s) - format as dd/mm/yyyy or dd mmm yy."));
             return;
        }
        g_date_add_days (&DateBefore,1);
        g_date_strftime(buffer, MAX_FILENAME_STRING, _("%x"), &DateBefore);
        setTimeFromDate(&tptr, &DateBefore);
        mSearchControl->before = mktime(&tptr);
        mSearchControl->flags |= SEARCH_BEFORE_SET;
        setTimeFromDate(&tptr, &DateAfter);
        mSearchControl->after = mktime(&tptr); 
        mSearchControl->flags |= SEARCH_AFTER_SET;
        break;
      }
      case 5: {
        time ( &rawtime );
        strftime(buffer, 80, "%x", localtime(&rawtime));/* don't change parameter %x */
        g_date_set_parse(&previous_date, buffer);
        /* we get the unit and entry value to substract to current date - thus wa get the 'after' limit*/
        current_since_unit = tmpSinceUnits;
        if(current_since_unit<1) 
               current_since_unit = 0;
        current_since_value = tmpSinceEntry; 
        if(current_since_value<1) 
               current_since_value = 1;
        switch(current_since_unit)
           {
             case 0:{g_date_subtract_days (&previous_date,current_since_value);break;}/* days */
             case 1:{g_date_subtract_days (&previous_date,current_since_value*7);break;}/* weeks */
             case 2:{g_date_subtract_months (&previous_date,current_since_value);break;}/* months */
             case 3:{g_date_subtract_months (&previous_date,current_since_value*3);break;}/* quarters */
             case 4:{g_date_subtract_years (&previous_date,current_since_value);break;}/* years */
             default:;
           }/* end switch */
        g_date_strftime(buffer, MAX_FILENAME_STRING, _("%x"), &previous_date);
        setTimeFromDate(&tptr, &previous_date);
        mSearchControl->after = mktime(&tptr); 
        mSearchControl->flags |= SEARCH_AFTER_SET;
        /* we get the current date - the current date is the before limit, of course*/
        time ( &rawtime );
        strftime(buffer, 80, "%x", localtime(&rawtime));/* don't change parameter %x */
        g_date_set_parse(&current_date, buffer);
        g_date_add_days (&current_date,1);
        g_date_strftime(buffer, MAX_FILENAME_STRING, _("%x"), &current_date);
        setTimeFromDate(&tptr, &current_date);
        mSearchControl->before = mktime(&tptr);
        mSearchControl->flags |= SEARCH_BEFORE_SET;
        break;
      }
      default:;
    }/* end switch */

 /* coherence tests - useless - Luc A feb 2018 - to remove - only useful for Interval mode */
 if((g_date_valid(&DateAfter)) && (g_date_valid(&DateBefore))) {
     if( (fBeforeCheck) && (fAfterCheck)) {
        gint cmp_date = g_date_compare (&DateAfter,&DateBefore);/* returns 1 if the first is wrong, i.e. after last, 0 if equal */
        if(cmp_date>=0) {
              miscErrorDialog(widget, _("<b>Error!</b>\n\nDates mismatch ! 'Before than' date must be <i>more recent</i> than 'After than' date.\n<b>Search can't proceed correctly !</b>\nPlease check the dates."));
              return;
        }
     }
 }/* endif */ 
}

/*
 * Internal helper: scans the form, and converts all text entries into internal strings/dates/integers etc.
 * For example if the user provides a file name, then the equivalent regex is stored locally
 */
void getSearchCriteria(GtkWidget *widget, searchControl *mSearchControl)
{
  mSearchControl->fileSearchIsRegEx = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(lookup_widget(widget, "regularExpressionRadioFile")));

  /* Grab the filename, containing, and look-in entries */

    mSearchControl->textSearchRegEx = (gchar *)gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT(lookup_widget(widget, "containingText")));
    mSearchControl->fileSearchRegEx = (gchar *)gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT(lookup_widget(widget, "fileName")));
    mSearchControl->startingFolder = comboBoxReadCleanFolderName(GTK_COMBO_BOX(lookup_widget(widget, "lookIn")));
}

/*
 * Internal helper: scans the form, and converts all checkboxes to their flag equivalent
 * For example, if the user unchecks the "recurse folders" toggle, then a flag needs to be set.
 */
void getSearchFlags(GtkWidget *widget, searchControl *mSearchControl)
{
  /* Set defaults here */
  mSearchControl->flags = 0; /* Disable all flags */
  mSearchControl->textSearchFlags = REG_NEWLINE; /* Force search to treat newlines appropriately */

  /* Store the search specific options */
 /* If expert mode */
    if ((gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(lookup_widget(widget, "containingTextCheck")))) &&
        (*mSearchControl->textSearchRegEx != '\0')) {
      mSearchControl->flags |= SEARCH_TEXT_CONTEXT; /* Allow context switching */
    }
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(lookup_widget(widget, "searchSubfoldersCheck")))) {
      mSearchControl->flags |= SEARCH_SUB_DIRECTORIES; /* Allow sub-directory searching */
    }

  /* Store the common options */
  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(lookup_widget(widget, "notExpressionCheckFile")))) {
      mSearchControl->flags |= SEARCH_INVERT_FILES; /* Invert the search on File names e.g. find everything but abc */
  }  
  if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(lookup_widget(widget, "matchCaseCheckFile")))) {
    mSearchControl->fileSearchFlags |= REG_ICASE;
  }  
  if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(lookup_widget(widget, "ignoreHiddenFiles")))) {
      mSearchControl->flags |= SEARCH_HIDDEN_FILES; /* Allow hidden searching */
  }  
  mSearchControl->fileSearchFlags |= REG_NOSUB;
  if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(lookup_widget(widget, "matchCaseCheckContents")))) {
    mSearchControl->textSearchFlags |= REG_ICASE;
  }
  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(lookup_widget(widget, "followSymLinksCheck"))) == FALSE) {
    mSearchControl->flags |= SEARCH_SKIP_LINK_FILES;
  }

  /* Store the user preferences */
  if (getExtendedRegexMode(widget)) {
    mSearchControl->fileSearchFlags |= REG_EXTENDED;
    mSearchControl->textSearchFlags |= REG_EXTENDED;
  }
}


/*************************************************************
 * Main search entry points (not threaded)
 *************************************************************/
/*
 * Callback helper: search started by GUI button press from user.
 * Creates storage arrays, validates user entries, and copies all
 * settings to memory before kicking off the POSIX search thread.
 * TODO: More validation is required at time of user entry to simplify this code.
 * TODO: User entry needs converting to UTF-8 valid text.
 * TODO: Create multiple functions from code, as this is definitely too long a function!
 */
void start_search_thread(GtkWidget *widget)
{
  //  GThread *threadId; /* Thread ID */
  searchControl *mSearchControl;
  GObject *window1 = G_OBJECT(mainWindowApp);
  gchar *tmpChar;
  guint tmpInt;
  gchar buffer[MAX_FILENAME_STRING + 1], *tmpStr;
  GKeyFile *keyString = getGKeyFile(widget);
  GtkWidget *dialog;
  GtkTreeView *listView;
  GtkListStore *sortedModel;

  /* clear flag */
  fStartedSearch = TRUE;
  /* Clear results prior to resetting data */
  if (getResultsViewHorizontal(widget)) {
    listView = GTK_TREE_VIEW(lookup_widget(widget, "treeview1"));
  } else {
    listView = GTK_TREE_VIEW(lookup_widget(widget, "treeview2"));
  }
  sortedModel = GTK_LIST_STORE(gtk_tree_model_sort_get_model(GTK_TREE_MODEL_SORT(gtk_tree_view_get_model(listView))));
  g_assert(sortedModel != NULL);
  gtk_list_store_clear(sortedModel);

  /* Create user data storage & control (automatic garbage collection) */
  createSearchData(window1, MASTER_SEARCH_DATA);
  createSearchControl(window1, MASTER_SEARCH_CONTROL);  
 
  mSearchControl = g_object_get_data(window1, MASTER_SEARCH_CONTROL);
  mSearchControl->cancelSearch = FALSE; /* reset cancel signal */
  
  /* Store the form data within mSearchControl */
  mSearchControl->widget = GTK_WIDGET(window1); /* Store pointer to the main windows */
  getSearchCriteria(widget, mSearchControl); /* Store the user entered criteria */
  getSearchFlags(widget, mSearchControl); /* Store the user set flags */
  getSearchExtras(mainWindowApp, mSearchControl); /* Store the extended criteria too */

  /* Test starting folder exists */
  if ((mSearchControl->startingFolder == NULL) || (*(mSearchControl->startingFolder) == '\0')) {
    dialog = gtk_message_dialog_new (GTK_WINDOW(window1),
                                     GTK_DIALOG_DESTROY_WITH_PARENT,
                                     GTK_MESSAGE_ERROR,
                                     GTK_BUTTONS_CLOSE,
                                     _("Error! Look In directory cannot be blank."));
    gtk_dialog_run (GTK_DIALOG (dialog));
    gtk_widget_destroy (dialog);
    return;
  }
  
  if (!g_file_test(mSearchControl->startingFolder, G_FILE_TEST_IS_DIR)) {
    dialog = gtk_message_dialog_new (GTK_WINDOW(window1),
                                     GTK_DIALOG_DESTROY_WITH_PARENT,
                                     GTK_MESSAGE_ERROR,
                                     GTK_BUTTONS_CLOSE,
                                     _("Error! Look In directory is invalid."));
    gtk_dialog_run (GTK_DIALOG (dialog));
    gtk_widget_destroy (dialog);
    return;
  }

  /* Test fileSearchRegEx reg expression is valid */
  if (mSearchControl->fileSearchIsRegEx) {
    if (test_regexp(mSearchControl->fileSearchRegEx, mSearchControl->fileSearchFlags, _("Error! Invalid File Name regular expression")) == FALSE) {
      return;
    }
  }

  /* Test textSearchRegEx reg expression is valid */
  if (test_regexp(mSearchControl->textSearchRegEx, mSearchControl->textSearchFlags, _("Error! Invalid Containing Text regular expression")) == FALSE) {
    return;
  }

  /* Store the text strings (once validated) into the combo boxes */

    addUniqueRow(lookup_widget(widget, "fileName"), mSearchControl->fileSearchRegEx);
    addUniqueRow(lookup_widget(widget, "containingText"), mSearchControl->textSearchRegEx);
    addUniqueRow(lookup_widget(widget, "lookIn"), mSearchControl->startingFolder);
  /* modifiy windows' title according to research criteria */
  gtk_window_set_title (GTK_WINDOW(window1), g_strdup_printf(_("Searchmonkey : search in %s/"), mSearchControl->startingFolder));/* Luc A - janv 2018 */
  /* create the search thread */
  g_thread_new ("walkDirectories",walkDirectories, window1); 

  //  threadId = g_thread_create (walkDirectories, window1, FALSE, NULL); 
}


/*
 * Callback helper: search stopped (at any time) by GUI button press from user.
 */
void stop_search_thread(GtkWidget *widget)
{
  searchControl *mSearchControl;
  GObject *window1 = G_OBJECT(lookup_widget(widget, "window1"));
  
  fStartedSearch = FALSE;
  mSearchControl = g_object_get_data(window1, MASTER_SEARCH_CONTROL);

  g_mutex_lock(&mutex_Control);
  mSearchControl->cancelSearch = TRUE;
  g_mutex_unlock(&mutex_Control);
 /* modifiy windows' title according to research criteria */
  gtk_window_set_title (GTK_WINDOW(window1), _("Aborting research-Searchmonkey"));/* Luc A - janv 2018 - the order if VOLONTARY inverted to keep attention */
}


/*************************************************************
 * Main search POSIX thread
 *************************************************************/
/*
 * POSIX thread entry point for main search loop.
 * This function takes pointer to main application window.
 * Returns 0 on sucess (always). 
 */
void *walkDirectories(void *args)
{
  GObject *object = args; /* Get GObject pointer from args */
  searchControl *mSearchControl; /* Master status bar */
  searchData *mSearchData; /* Master search data */
  statusbarData *status; /* Master status bar */
  glong matchCount;
  GtkTreeView *listView=NULL;
  GtkTextView *textView=NULL; 
  GtkListStore *sortedModel=NULL;
  GtkTextBuffer *textBuffer=NULL;

  g_mutex_lock(&mutex_Data);
  gdk_threads_enter ();
  mSearchData = g_object_get_data(object, MASTER_SEARCH_DATA);
  mSearchControl = g_object_get_data(object, MASTER_SEARCH_CONTROL);
  status = g_object_get_data(object, MASTER_STATUSBAR_DATA);
  gdk_threads_leave ();

  g_assert(mSearchData != NULL);
  g_assert(mSearchControl != NULL);
  g_assert(status != NULL);

  /* Disable go button, enable stop button.. */
  gdk_threads_enter ();
  gtk_widget_set_sensitive(lookup_widget(mSearchControl->widget, "playButton1"), FALSE);
  gtk_widget_set_sensitive(lookup_widget(mSearchControl->widget, "playButton3"), FALSE);/* menu item */
  gtk_widget_set_sensitive(lookup_widget(mSearchControl->widget, "stopButton1"), TRUE);
  gtk_widget_set_sensitive(lookup_widget(mSearchControl->widget, "stopButton3"), TRUE);
  gtk_widget_set_sensitive(lookup_widget(mSearchControl->widget, "save_results1"), FALSE);
  gdk_threads_leave ();
  
  if (getResultsViewHorizontal(mSearchControl->widget)) {
    listView = GTK_TREE_VIEW(lookup_widget(mSearchControl->widget, "treeview1"));
    textView = GTK_TEXT_VIEW(lookup_widget(mSearchControl->widget, "textview1"));
  } else {
    listView = GTK_TREE_VIEW(lookup_widget(mSearchControl->widget, "treeview2"));
    textView = GTK_TEXT_VIEW(lookup_widget(mSearchControl->widget, "textview4"));
  }

  textBuffer = gtk_text_view_get_buffer (textView);
  /* it seems to work withou segfault if protected under a thread  - Luc A. 12 avr 2018 */
  gdk_threads_enter ();
  gtk_text_buffer_set_text(textBuffer, " ", 1); /* Clear text! Magistral bug - Luc A 11 avr 2018 */
  gdk_threads_leave ();

  sortedModel = GTK_LIST_STORE(gtk_tree_model_sort_get_model(GTK_TREE_MODEL_SORT(gtk_tree_view_get_model(listView))));
  g_assert(sortedModel != NULL);
  mSearchData->store = sortedModel;
  matchCount = phaseOneSearch(mSearchControl, mSearchData, status);

  if (matchCount > 0) {
    gboolean phasetwo;
    
    g_mutex_lock(&mutex_Control);
    phasetwo = ((mSearchControl->cancelSearch == FALSE) && ((mSearchControl->flags & SEARCH_TEXT_CONTEXT) != 0));
    g_mutex_unlock(&mutex_Control);

    if (phasetwo) {
      matchCount = phaseTwoSearch(mSearchControl, mSearchData, status); 
    }
  }
  updateStatusFilesFound(matchCount, status, mSearchControl);

/* Re-enable the go button */
  gdk_threads_enter ();
  gtk_widget_set_sensitive(lookup_widget(mSearchControl->widget, "playButton1"), TRUE);
  gtk_widget_set_sensitive(lookup_widget(mSearchControl->widget, "playButton3"), TRUE);
  gtk_widget_set_sensitive(lookup_widget(mSearchControl->widget, "stopButton1"), FALSE);
  gtk_widget_set_sensitive(lookup_widget(mSearchControl->widget, "stopButton3"), FALSE);
  if (matchCount > 0) {
      gtk_widget_set_sensitive(lookup_widget(mSearchControl->widget, "save_results1"), TRUE);
  }
  gdk_threads_leave ();
  fStartedSearch = FALSE;
  g_mutex_unlock(&mutex_Data);
  g_thread_exit (GINT_TO_POINTER(0));
}


/*
 * POSIX threaded: phase one of main search loop.
 * This function searches for all of the matching file names in the specified folder.
 * Returns number of file matches found.
 */
glong phaseOneSearch(searchControl *mSearchControl, searchData *mSearchData, statusbarData *status)
{
  GtkStatusbar *statusbar = GTK_STATUSBAR(lookup_widget(mSearchControl->widget, "statusbar1"));
  GPtrArray *scanDirStack; /* Pointer to the current open directory lister */
  GPtrArray *curDirStack; /* Pointer to the current folder base name (as a stack) */
  gchar *tmpFileName; /* Pointer to the current file name or sub-directory name returned by scanDirStack */
  gchar *tmpFullFileName; /* Pointer to the full filename or directory provided by */
  regex_t searchRegEx;
  GPatternSpec *searchGlob;
  gchar *tString, *pStrChunk;
  glong matchCount = 0;
  GTimeVal phaseOneStartTime, phaseOneMidTime;
  GtkProgressBar *pbar = GTK_PROGRESS_BAR(lookup_widget(GTK_WIDGET(mainWindowApp), "progressbar1"));
  gint i = 0, Icount = 0, result;
  gdouble deltaTime;
  gboolean stopSearch, symlink = FALSE; /* TRUE if file currently being tested is a symlink */
  GError *dirOpenError = NULL;
  char *pCurDirStack;

  /* Compile the regular expression */
  if (mSearchControl->fileSearchIsRegEx) {
    regcomp(&searchRegEx, mSearchControl->fileSearchRegEx, mSearchControl->fileSearchFlags);
  } else {
    if ((mSearchControl->fileSearchFlags & REG_ICASE )== 0) {
      searchGlob = g_pattern_spec_new(mSearchControl->fileSearchRegEx);
    } else {
      searchGlob = g_pattern_spec_new(g_utf8_strdown(mSearchControl->fileSearchRegEx, -1));
    }
  }
    
  /* Initialise stacks */
  curDirStack = g_ptr_array_new();
  scanDirStack = g_ptr_array_new(); 
 
  /* Copy base directory out */
  pStrChunk = g_string_chunk_insert_const(mSearchData->locationChunk, mSearchControl->startingFolder);
  g_ptr_array_add(curDirStack, pStrChunk);
  g_ptr_array_add(scanDirStack, g_dir_open(pStrChunk, 0, NULL));

  g_snprintf(status->constantString, MAX_FILENAME_STRING, _("Phase 1 searching %s"), pStrChunk);
  gdk_threads_enter ();
  gtk_statusbar_push(statusbar, STATUSBAR_CONTEXT_ID(statusbar),
                     g_string_chunk_insert(status->statusbarChunk, status->constantString));
  gdk_threads_leave ();
  
  g_mutex_lock(&mutex_Control);
  stopSearch = mSearchControl->cancelSearch;
  g_mutex_unlock(&mutex_Control);

  while ((curDirStack->len > 0) && (stopSearch == FALSE)) {
    if (((mSearchControl->flags & SEARCH_TEXT_CONTEXT) == 0) && 
         ((mSearchControl->flags & LIMIT_RESULTS_SET) != 0) && 
	   (matchCount == mSearchControl->limitResults)) {
      break;
    }

    /* Test how long this process has been going for after a few loops to calculate the break point */
    i++;
    if (i == PBAR_SHOW_IF_MIN) {
      g_get_current_time (&phaseOneStartTime);
    } else if (i == (2 * PBAR_SHOW_IF_MIN)) {
      g_get_current_time (&phaseOneMidTime);
      /* Calculate number of loops to pulse bar every 500ms/MULTIPLIER */
      gdouble deltaTime = (phaseOneMidTime.tv_sec - phaseOneStartTime.tv_sec);
      deltaTime += ((gdouble)(phaseOneMidTime.tv_usec - phaseOneStartTime.tv_usec) / 1000000);
      deltaTime = PBAR_SHOW_IF_MIN/(PHASE_ONE_PBAR_PULSE_MULTILPIER * deltaTime);
      Icount = (gint)(deltaTime);
      gdk_threads_enter ();
      gtk_progress_bar_set_text(pbar, ""); /* Clear it first*/
      gdk_threads_leave ();
    }

    /* Once started, pulse until done */
    if (Icount > 0) {
      if ((i % Icount) == 0) {
        gdk_threads_enter ();
        gtk_progress_bar_pulse(pbar);
        gdk_threads_leave ();
      }
    }
    
    /* Get next file from the current scan directory */
    tmpFileName = g_strdup(g_dir_read_name(GET_LAST_PTR(scanDirStack)));
    if (tmpFileName == NULL) {      
      gdk_threads_enter ();
      gtk_statusbar_pop(statusbar, STATUSBAR_CONTEXT_ID(statusbar));
      gdk_threads_leave ();
      g_dir_close(DEL_LAST_PTR(scanDirStack));
      DEL_LAST_PTR(curDirStack);
      g_free(tmpFileName);
      continue;
    }

    /* Check if file name is actually a folder name */
    pCurDirStack = GET_LAST_PTR(curDirStack);
    if ((pCurDirStack[0] == '/') && (pCurDirStack[1] == '\0')) {
      tmpFullFileName = g_strconcat(pCurDirStack, tmpFileName, NULL);
    } else {
      tmpFullFileName = g_strconcat(pCurDirStack, G_DIR_SEPARATOR_S, tmpFileName, NULL);
    }

    /* Replace data with symbolic link*/
    if (g_file_test(tmpFullFileName, G_FILE_TEST_IS_SYMLINK)) {
      if ((mSearchControl->flags & SEARCH_SKIP_LINK_FILES) == 0) {
        symlink = symLinkReplace(&tmpFullFileName, &tmpFileName);
        if (!symlink) { /* not a valid symlink, so skip through */
          g_free(tmpFullFileName);
          g_free(tmpFileName);
          continue;
        }
      } else { /* User requested to skip symlinks */
        g_free(tmpFullFileName);
        g_free(tmpFileName);
        continue;
      }
    }
      
    /* Check for hidden files (unless overriden by user) */
    if ((mSearchControl->flags & SEARCH_HIDDEN_FILES) == 0) {
      if (*tmpFileName == '.') {
        g_free(tmpFullFileName);
        g_free(tmpFileName);
        continue;
      }
    }
    
    /* Start working with the new folder name */
    if (g_file_test(tmpFullFileName, G_FILE_TEST_IS_DIR)) {
      if (((mSearchControl->flags & SEARCH_SUB_DIRECTORIES) != 0) && 
          (((mSearchControl->flags & DEPTH_RESTRICT_SET) == 0) ||
           (mSearchControl->folderDepth > (curDirStack->len -1)))) {
        g_snprintf(status->constantString, MAX_FILENAME_STRING, _("Phase 1 searching %s"), tmpFullFileName);
        gdk_threads_enter ();
        gtk_statusbar_push(statusbar, STATUSBAR_CONTEXT_ID(statusbar),
                           g_string_chunk_insert(status->statusbarChunk, status->constantString));
        gdk_threads_leave ();
        tString = g_string_chunk_insert_const(mSearchData->locationChunk, tmpFullFileName);
        g_ptr_array_add(curDirStack, tString);
        g_ptr_array_add(scanDirStack, g_dir_open(tString, 0, &dirOpenError));
        if (GET_LAST_PTR(scanDirStack) == NULL) {
          g_print(_("%s %s\n"), dirOpenError->message, tString);
          g_clear_error(&dirOpenError);
          DEL_LAST_PTR(curDirStack);
          DEL_LAST_PTR(scanDirStack);
        }
      }
    } else { /* Otherwise, a filename has been found */
      if (mSearchControl->fileSearchIsRegEx) {
        
        result = regexec(&searchRegEx, tmpFileName, 0, NULL, 0);
      } else {
        gchar * tmpMatchString;
        if ((mSearchControl->fileSearchFlags & REG_ICASE) ==0) {
          tmpMatchString = g_filename_to_utf8(tmpFileName, -1, NULL, NULL, NULL);
        } else {
          tmpMatchString = g_utf8_strdown(g_filename_to_utf8(tmpFileName, -1, NULL, NULL, NULL), -1);
        }
        if (g_pattern_match(searchGlob, g_utf8_strlen(tmpMatchString, -1), tmpMatchString, NULL)) {
          result = 0;
        } else {
          result = 1;
        }
      }

      if ((((mSearchControl->flags & SEARCH_INVERT_FILES) == 0) && (result == 0)) ||
          (((mSearchControl->flags & SEARCH_INVERT_FILES) != 0) && (result != 0))) {
        if (statMatchPhase(tmpFullFileName, mSearchControl)) {
          g_ptr_array_add(mSearchData->fullNameArray, g_strdup(tmpFullFileName));
          g_ptr_array_add(mSearchData->fileNameArray, g_strdup(tmpFileName));
          g_ptr_array_add(mSearchData->pLocationArray, GET_LAST_PTR(curDirStack));
          matchCount ++;
          if ((mSearchControl->flags & SEARCH_TEXT_CONTEXT) == 0) { 
            displayQuickMatch(mSearchControl, mSearchData);
          }
        }
      }
    }

    /* Update cancel search detection, free tmp strings, and reset symlink flag */
    g_mutex_lock(&mutex_Control);
    stopSearch = mSearchControl->cancelSearch;
    g_mutex_unlock(&mutex_Control);
    g_free(tmpFullFileName);
    g_free(tmpFileName);
    symlink = FALSE;
  }

  /* Clean up status/progress bar */
  g_mutex_lock(&mutex_Control);
  stopSearch = mSearchControl->cancelSearch;
  g_mutex_unlock(&mutex_Control);

  if (((mSearchControl->flags & SEARCH_TEXT_CONTEXT) == 0) ||
      (stopSearch == TRUE)){
    updateStatusFilesFound(matchCount, status, mSearchControl);
  }
  gdk_threads_enter ();
  gtk_progress_bar_set_fraction(pbar, 0);
  gdk_threads_leave ();
  

  /* Clean up memory bar now! */
  g_ptr_array_free(curDirStack, TRUE);
  g_ptr_array_free(scanDirStack, TRUE);
  if (mSearchControl->fileSearchIsRegEx) {
    regfree(&searchRegEx);
  } else {
    g_pattern_spec_free(searchGlob);
  }
  return matchCount;
}



/*
 * POSIX threaded: phase two of main search loop.
 * This function searches for contents within each of the files found in phase one.
 * Returns number of content matches found. Always <= phase 2 match count.
 */
glong phaseTwoSearch(searchControl *mSearchControl, searchData *mSearchData, statusbarData *status)
{
  GtkStatusbar *statusbar = GTK_STATUSBAR(lookup_widget(mSearchControl->widget, "statusbar1"));
  glong matchCount=0;
  gint i, spinButtonValue = 0;
  gsize length;
  gchar *tmpFileName, *contents;
  regex_t search;
  textMatch *newMatch;
  GtkProgressBar *pbar = GTK_PROGRESS_BAR(lookup_widget(GTK_WIDGET(mainWindowApp), "progressbar1"));
  gint pbarNudge;
  gdouble pbarNudgeCount = 0, pbarIncrement;
  gchar pbarNudgeCountText[6]; /* Stores "100%" worst case.. */
  gboolean stopSearch;
  gchar *tmpExtractedFile = NULL; /* the gchar* to switch the filenames if it's an Office File */
  gboolean fDeepSearch = FALSE; /* special flag armed when we search inside complex files like Docx, ODT, PDF ... in order to keep the "true" filename for status bar */
  gboolean fIsOffice=FALSE; /* flag if we found an office style file */
  struct stat buf;
  
  if(mSearchData->fullNameArray->len > 100) {
     pbarNudge = ((gdouble)mSearchData->fullNameArray->len / 100);  /* Every pbarNudge - increment 1/100 */
     pbarIncrement = (gdouble)pbarNudge / (gdouble)mSearchData->fullNameArray->len;
  } 
  else {
        pbarNudge = 1; /* For every file, increment 1/MAX*/
        pbarIncrement = 1 / (gboolean)mSearchData->fullNameArray->len;
  }/* endif */
  
  /* Update the status bar */
  g_snprintf(status->constantString, MAX_FILENAME_STRING, _("Phase 2 starting..."));
  gdk_threads_enter ();
  gtk_statusbar_pop(statusbar, STATUSBAR_CONTEXT_ID(statusbar));
  gtk_statusbar_push(statusbar, STATUSBAR_CONTEXT_ID(statusbar),
                     status->constantString);
  gdk_threads_leave ();

  /* Compile the regular expression */
  regcomp(&search, mSearchControl->textSearchRegEx, mSearchControl->textSearchFlags);
  
  gdk_threads_enter ();
  gtk_progress_bar_set_fraction(pbar, 0);
  gdk_threads_leave ();

  /* Loop through all files, and get all of the matches from each */
  for (i=0; i<(mSearchData->fullNameArray->len); i++) 
    {
      if (((mSearchControl->flags & LIMIT_RESULTS_SET) != 0) && (matchCount == mSearchControl->limitResults)) {
         break;
      }
      /* Increment progress bar whenever pbarNudge files have been searched */
      if (((i+1) % pbarNudge) == 0) {
        g_sprintf(pbarNudgeCountText, "%.0f%%", pbarNudgeCount * 100);
        gdk_threads_enter ();
        gtk_progress_bar_set_fraction(pbar, pbarNudgeCount);
        gtk_progress_bar_set_text(pbar, pbarNudgeCountText);
        gdk_threads_leave ();
        pbarNudgeCount += pbarIncrement;
      }/* endif */
    
      tmpFileName = g_strdup_printf("%s", (gchar*)g_ptr_array_index(mSearchData->fullNameArray, i) );/* modified Luc A janv 2018 */
      /* We must check the type-file in order to manage non pure text files like Office files 
       Luc A. 7 janv 2018 */
      /* if filesize > maxfile size we skip phase two, for example for video files, iso files and so on ! */
      lstat(tmpFileName, &buf); /* Replaces the buggy GLIB equivalent */
      if(buf.st_size>MAX_FILE_SIZE_PHASE_TWO) { 
          printf("* File size of %s too large for safe containing search *\n", tmpFileName); 
          break;
      }
      switch(get_file_type_by_signature(tmpFileName)) {
       case iXmlOdpFile: /* Impress */
       case iXmlOdsFile: /* calc */ 
       case iXmlOdtFile: { /* OPenDocument Text ? */
         tmpExtractedFile = ODTCheckFile((gchar*)tmpFileName,  GetTempFileName("monkey")  );
         if(tmpExtractedFile!=NULL) {  
           fDeepSearch = TRUE;
           fIsOffice = TRUE;
         }
         break;
       }
       case iXmlMsWordFile: {/* DOCX from MSWord 2007 and newer ? */
         tmpExtractedFile = DocXCheckFile((gchar*)tmpFileName,  GetTempFileName("monkey")  );
         if(tmpExtractedFile!=NULL) {
           fDeepSearch = TRUE;
           fIsOffice = TRUE;
         }
         break;
       }
       case iOleMsWordFile: {/* Ms Word 6/95/97>>2003 ? */
         tmpExtractedFile = OLECheckFile((gchar*)tmpFileName,  GetTempFileName("monkey")  );
         if(tmpExtractedFile!=NULL) {
           fDeepSearch = TRUE;
           fIsOffice = TRUE;
         }
         break;
       }
       case iMsWriteFile: {/* Old Ms Write from 90's ? */
         tmpExtractedFile = WRICheckFile((gchar*)tmpFileName,  GetTempFileName("monkey")  );
         if(tmpExtractedFile!=NULL) {
           fDeepSearch = TRUE;
           fIsOffice = TRUE;
         }
         break;
       }
       case iOldMSWordFile: {/* Old Ms WinWord from 90's ? */
         tmpExtractedFile = WRDCheckFile((gchar*)tmpFileName,  GetTempFileName("monkey")  );
         if(tmpExtractedFile!=NULL) {
           fDeepSearch = TRUE;
           fIsOffice = TRUE;
         }
         break;
       }
       case iPdfFile: {  /* Acrobat PDF  ? */
         tmpExtractedFile = PDFCheckFile((gchar*)tmpFileName,  GetTempFileName("monkey")  );
         if(tmpExtractedFile!=NULL) {
           fDeepSearch = TRUE;
           fIsOffice = TRUE;
         }
         break;
       }
       case iAbiwordFile: {/* Abiword ? */
         tmpExtractedFile = ABWCheckFile((gchar*)tmpFileName,  GetTempFileName("monkey")  );
         if(tmpExtractedFile!=NULL) {
           fDeepSearch = TRUE;
           fIsOffice = TRUE;
         }
         break;
       }
       case iRtfFile: {  /* Rich Text Format - RTF  ? */
         tmpExtractedFile = RTFCheckFile((gchar*)tmpFileName,  GetTempFileName("monkey")  );
         if(tmpExtractedFile!=NULL) {
           fDeepSearch = TRUE;
           fIsOffice = TRUE;
         }
         break;
       }
       default :{
         fIsOffice = FALSE;
       }
     }/* end switch */   
  
    /* Update the status bar */
    gdk_threads_enter ();
    g_snprintf(status->constantString, MAX_FILENAME_STRING, _("Phase 2 searching %s"), tmpFileName);
    if(fDeepSearch==TRUE)  {
           if(tmpFileName!=NULL)
               g_free(tmpFileName);
           tmpFileName = g_strdup_printf("%s", tmpExtractedFile);
           g_free(tmpExtractedFile);
           fDeepSearch = FALSE;
    }
    gtk_statusbar_pop(statusbar, STATUSBAR_CONTEXT_ID(statusbar));
    gtk_statusbar_push(statusbar, STATUSBAR_CONTEXT_ID(statusbar),
                       status->constantString);
    gdk_threads_leave ();
    
    /* Open file (if valid) */
    /* i's a wrapper for Glib's g_file_get_contents() */
    /* in 'contents" string variable we have the content read from file */
//printf("phase two avant test contents2  ligne 2396 \n");
    if(g_file_get_contents2(tmpFileName, &contents, &length, NULL)) {
      /* Try to get a match line 954*/
      if(getAllMatches(mSearchData, contents, length, &search, fIsOffice)) {        
        /* get full filename pointer */
        newMatch = GET_LAST_PTR(mSearchData->textMatchArray);
        newMatch->pFullName = g_ptr_array_index(mSearchData->fullNameArray, i);
        newMatch->pFileName = g_ptr_array_index(mSearchData->fileNameArray, i);
        newMatch->pLocation = g_ptr_array_index(mSearchData->pLocationArray, i);
        // newMatch->fileSize = length;/* be careful for Office Files */
        getFileSize(mSearchData, newMatch);/* added by Luc A feb 2018 */
        getLength(mSearchData, newMatch);      
        getFileType(mSearchData, newMatch);
        getModified(mSearchData, newMatch);        
        /* Convert the absolutes to relatives */
        spinButtonValue = mSearchControl->numExtraLines;
        dereferenceAbsolutes(mSearchData, contents, length, spinButtonValue);
        /* Display the matched string */
        displayMatch(mSearchControl, mSearchData);/* function is line 1765*/        
        /* Increment the match counter */
        matchCount++;
      }
      if(tmpFileName!=NULL)
          g_free(tmpFileName);
      g_free(contents); /* Clear file contents from memory */
      g_mutex_lock(&mutex_Control);
      stopSearch = mSearchControl->cancelSearch;
      g_mutex_unlock(&mutex_Control);
      if (stopSearch == TRUE) {
        break;
      }
    }
  }/* for next i */

  /* Update statusbar/progress bar when done */
  updateStatusFilesFound(matchCount, status, mSearchControl);
  gdk_threads_enter ();
  gtk_progress_bar_set_fraction(pbar, 0);
  gtk_progress_bar_set_text(pbar, "");
  gdk_threads_leave ();

  /* Clean exit */
  regfree(&search);
  return matchCount;
}


/*
 * If valid symbolic link, replace (full) file name with real folder/file.
 * If not directory and valid symlink, return TRUE.
 * Otherwise, do not change (full) file name data, and return FALSE.
 */
gboolean symLinkReplace(gchar **pFullFileName, gchar **pFileName)
{
  gchar * tmpSymFullFileName = g_file_read_link(*pFullFileName, NULL);
  gchar ** tmpSymSplit = g_strsplit(tmpSymFullFileName, G_DIR_SEPARATOR_S, -1);
  guint splitLength = g_strv_length(tmpSymSplit);
  gboolean retVal = FALSE;
  
  /* If symlink splits into 1 or more parts, replace filename(s) with new symbolic link details */
  if (splitLength != 0) {
    g_free(*pFullFileName);
    g_free(*pFileName);
    (*pFileName) = g_strdup(tmpSymSplit[splitLength - 1]); /* Copy last part as filename data */
    (*pFullFileName) = tmpSymFullFileName;
    retVal = TRUE;
  }

  /* otherwise tmpFullFileName is a directory, so keep previous data */
  g_strfreev(tmpSymSplit);
  return retVal;
}


/*
 * POSIX threaded: phase two helper function.
 * Searches through complete text looking for regular expression matches.
 * contents = gchar buffer with all text in the file open 
 * Returns TRUE if >1 match found.
 */
gboolean getAllMatches(searchData *mSearchData, gchar *contents, gsize length, regex_t *search, gboolean fOffice /*, gint index*/ )
{
  regmatch_t subMatches[MAX_SUB_MATCHES];
  gint tmpConOffset = 0;
  gchar *tmpCon;
  lineMatch *newLineMatch = NULL;
  textMatch *newTextMatch = NULL;
  
  tmpCon = contents;

  /* Loop through getting all of the absolute match positions */
  while(regexec(search, tmpCon, MAX_SUB_MATCHES, subMatches, 0) == 0) {
    if ((subMatches[0].rm_so == 0) && (subMatches[0].rm_eo == 0)) {
      break;
    }
    
    if(newTextMatch == NULL) {
      newTextMatch = g_malloc(sizeof(textMatch));
      newTextMatch->matchIndex = mSearchData->lineMatchArray->len; /* pre-empt new line being added */
      newTextMatch->matchCount = 0;
      g_ptr_array_add(mSearchData->textMatchArray, newTextMatch);
    }

    newLineMatch = g_malloc(sizeof(lineMatch));
    newLineMatch->fOfficeFile = fOffice;
    newLineMatch->pLine = NULL;
    newLineMatch->lineCount = 0;
    newLineMatch->lineLen = -1;
    newLineMatch->lineNum = -1;
    newLineMatch->offsetStart = tmpConOffset + subMatches[0].rm_so;
    newLineMatch->offsetEnd = tmpConOffset + subMatches[0].rm_eo;
    newLineMatch->invMatchIndex = (mSearchData->textMatchArray->len - 1); /* create reverse pointer */
    g_ptr_array_add(mSearchData->lineMatchArray, newLineMatch);
    (newTextMatch->matchCount) ++;
    
    tmpCon += subMatches[0].rm_eo;
    tmpConOffset += subMatches[0].rm_eo;
    if (tmpConOffset >= length) {
      break;
    }
  }/* wend */

  if (newTextMatch == NULL) {
    return FALSE;
  }
  return TRUE;  
}


/*
 * POSIX threaded: phase two helper function.
 * Converts regular expression output into actual string/line matches within the text.
 * numlines = number of extralines besides the matches
 */
void dereferenceAbsolutes(searchData *mSearchData, gchar *contents, gsize length, gint numLines)
{
  gsize lineCount = 0;
  gchar *tmpCon = contents;
  gchar *lineStartPtr = tmpCon; /* Initialise it.. */
  gsize currentOffset = 0;
  gsize lineOffset = 1; /* Initialise at the starting char */
  gsize currentMatch = 0;
  gsize lineNumber = 0;
  gsize lineStart = 0;
  gchar *tmpString2;
  gboolean needLineEndNumber = FALSE;
//printf("derefence avant GET_LAST \n");
  textMatch *textMatch = GET_LAST_PTR(mSearchData->textMatchArray);
  lineMatch *prevLineMatch, *newLineMatch = g_ptr_array_index(mSearchData->lineMatchArray,
                                              textMatch->matchIndex);
  gsize absMatchStart = newLineMatch->offsetStart;
  gsize absMatchEnd = newLineMatch->offsetEnd;
  gint i = 0, j=0;
  gchar *displayStartPtr = NULL, *displayEndPtr = NULL;
//printf("length dans search derefence =%d\n", length);
  /* Loop through whole file contents, one char at a time */
  while (currentOffset < length) { 

    /* Detect match start offset found - record localised stats */
    if (currentOffset == absMatchStart) {
      newLineMatch->offsetStart = (lineOffset - 1);
      newLineMatch->lineNum = (lineCount + 1);
    }
    
    /* Detect match end offset found - record localised stats */
    if (currentOffset == absMatchEnd) {
      newLineMatch->offsetEnd = (lineOffset - 1);
      newLineMatch->lineCount = ((lineCount - newLineMatch->lineNum) + 2);
      needLineEndNumber = TRUE;
    }
    
    if ((*tmpCon == '\n') || (*tmpCon == '\r') || (currentOffset >= (length - 1))) {
      if (needLineEndNumber) {
        newLineMatch->lineLen = lineOffset;
        if (lineStartPtr == NULL) {
          g_print ("%s: Error line %d, %d:%d", (gchar*)GET_LAST_PTR(mSearchData->fullNameArray),
                  (gint) lineCount,(gint) newLineMatch->offsetStart,(gint) newLineMatch->offsetStart);
        }

	displayStartPtr = lineStartPtr;
	i = 0;
	while ((displayStartPtr > contents) && (i <= numLines)) {
	  displayStartPtr--;
	  if (*displayStartPtr == '\n') {
	    i++;
	  }
	}
	if (displayStartPtr != contents) {
	  displayStartPtr++; /* Since otherwise it is on a \n */
	}
	else {
	  i++; /*Since here the first line hasn't been counted */
	}
	newLineMatch->lineCountBefore = i;/* number of 'real' lines available besides matches, i<=numlines */
// printf("* numlines = %d, mis %d lignes autour *\n", numLines, i);

	displayEndPtr = lineStartPtr + newLineMatch->lineLen - 1;/* to be converted to a true # of bytes !!! */
	i = 0;
	while ((displayEndPtr <= (contents + length - 1)) && (i < numLines)) {
	  displayEndPtr++;
	  if (*displayEndPtr == '\n') {
	    i++;
	  }
	}
	if (displayEndPtr != contents+length - 1) {
	  displayEndPtr--;
	}
	newLineMatch->lineCountAfter = i;

        tmpString2 = g_strndup (displayStartPtr, (displayEndPtr - displayStartPtr) + 1);
        /* we must check if there is a EOL char in the string ! */
       // for(j=0;j<=strlen(tmpString2);j++) {
         // if(tmpString2[j]=='\n')
           //   tmpString2[j]=' ';
        //}

        newLineMatch->pLine = g_string_chunk_insert(mSearchData->textMatchChunk, tmpString2);
        g_free(tmpString2);

        prevLineMatch = newLineMatch;

        if (++currentMatch >= textMatch->matchCount) {
          break; /* All matches are actioned - done! */
        }
        newLineMatch = g_ptr_array_index(mSearchData->lineMatchArray,
                                         (textMatch->matchIndex + currentMatch)); /* Move pointer on one! */
        absMatchStart = newLineMatch->offsetStart;
        absMatchEnd = newLineMatch->offsetEnd;
        
        /* If next match is on that same line -- rewind the pointers */
        if (absMatchStart <= currentOffset) {
          currentOffset -= prevLineMatch->lineLen;
          tmpCon -= prevLineMatch->lineLen;          
          lineCount -= prevLineMatch->lineCount;
        }
        

        needLineEndNumber = FALSE;
            
      }
      lineCount ++;
      lineOffset = 0;
      lineStart = currentOffset;
      lineStartPtr = (tmpCon + 1); /* First charactor after the newline */
    }
    
    tmpCon ++;
    lineOffset++;
    currentOffset ++;
  }
  return;
}


/*
 * POSIX threaded: phase two helper function.
 * Converts file size (gsize) into human readable string.
 */
void getLength(searchData *mSearchData, textMatch *newMatch)
{
  gchar *tmpString = NULL;


// printf("taille newmatch %10.2f\n", (float)newMatch->fileSize);
  if (newMatch->fileSize < 1024) {
    tmpString = g_strdup_printf(_("%d bytes"), newMatch->fileSize );
  } else if (newMatch->fileSize < (1024 * 1024)) {
    tmpString = g_strdup_printf (_("%1.1f KB"), ((float)newMatch->fileSize / 1024));
  } else {
    tmpString = g_strdup_printf (_("%1.1f MB"), ((float)newMatch->fileSize / (1024*1024)));
  }
  newMatch->pFileSize = (g_string_chunk_insert_const(mSearchData->fileSizeChunk, tmpString));
  g_free(tmpString);
}


/*
 * POSIX threaded: phase two helper function.
 * Converts filename into human readable string extension type.
 */
void getFileType(searchData *mSearchData, textMatch *newMatch)
{
  textMatch *textMatch = GET_LAST_PTR(mSearchData->textMatchArray);
  gchar *tmpChar = textMatch->pFileName;
  gchar *tmpString = NULL;
  
  /* Find end of string */
  while (*tmpChar != '\0') {
    tmpChar ++;
  }
  
  /* Find string extension */
  while (tmpChar > textMatch->pFileName) {
    tmpChar --;
    if (*tmpChar == '.') { 
      tmpChar++;
      tmpString = g_strdup_printf (_("%s-type"), tmpChar);
      newMatch->pFileType = (g_string_chunk_insert_const(mSearchData->fileTypeChunk, tmpString));
      newMatch->pShortFileType = (g_string_chunk_insert_const(mSearchData->shortfileTypeChunk, g_strdup_printf ("%s", tmpChar)));/* Luc A March 2018 */
      g_free(tmpString);
      return;
    } else {
      if (!g_ascii_isalnum(*tmpChar) && (*tmpChar != '~')) {
        break; /* Unexpected type - set to unknown */
      } 
    }
  }
  newMatch->pFileType = (g_string_chunk_insert_const(mSearchData->fileTypeChunk, _("Unknown")));
  newMatch->pShortFileType = (g_string_chunk_insert_const(mSearchData->shortfileTypeChunk, _("?")));
}


/*
 * POSIX threaded: phase two helper function.
 * Converts file modified date into human readable string date using user's locale settings.
 */
void getModified(searchData *mSearchData, textMatch *newMatch)
{
  struct stat buf; /* ce sont des structures propres au C dans les libs Stat et time */
  gint stringSize;
  gchar *tmpString;
  gchar buffer[80];/* added by Luc A., 27/12/2017 */
  textMatch *textMatch = GET_LAST_PTR(mSearchData->textMatchArray);
  
  lstat(textMatch->pFullName, &buf); /* Replaces the buggy GLIB equivalent */
  /* added by Luc A., 27 dec 2017 */
  strftime(buffer, 80, "%Ec", localtime(&(buf.st_mtime)));
  tmpString = g_strdup_printf ("%s",buffer);
  /* end addition by Luc A., 27/12/2017 */
 // tmpString = g_strdup_printf ("%s", asctime (localtime ( &(buf.st_mtime) )));/* st_mtime vient de la struture de type 'tm' de stat.h et time.h = original version by Adam C.*/
  stringSize = g_strlen(tmpString);
  if (tmpString[stringSize - 1] == '\n') {
    tmpString[stringSize - 1] = '\0';
  }
  
  newMatch->mDate = buf.st_mtime;
  newMatch->pMDate = (g_string_chunk_insert_const(mSearchData->mDateChunk, tmpString));
  g_free(tmpString);
}


/*
 * POSIX threaded: phase one helper function.
 * Get file size using lstat
 */
void getFileSize(searchData *mSearchData, textMatch *newMatch)
{
  struct stat buf;

  lstat(newMatch->pFullName, &buf); /* Replaces the buggy GLIB equivalent */
  newMatch->fileSize = buf.st_size;
}


/*
 * POSIX threaded: phase one/two helper function.
 * Updates the Gtk Statusbar with the search conclusion results.
 */
void updateStatusFilesFound(const gsize matchCount, statusbarData *status, searchControl *mSearchControl)
{
  GtkStatusbar *statusbar = GTK_STATUSBAR(lookup_widget(mSearchControl->widget, "statusbar1"));
  gboolean stopSearch;
  const gint tmpLimit = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(lookup_widget(mSearchControl->widget, "maxHitsSpinResults")));
  /* Update statusbar with new data */
  gdk_threads_enter();
  if (matchCount == 1) {
    g_snprintf(status->constantString, MAX_FILENAME_STRING, _("%d file found"), (gint) matchCount);
  } else {
    g_snprintf(status->constantString, MAX_FILENAME_STRING, _("%d files found"),(gint) matchCount);
  }
  if ((mSearchControl->flags & SEARCH_INVERT_FILES) != 0) {
    g_strlcat(status->constantString, _(" [inv]"), MAX_FILENAME_STRING);
  }
  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(lookup_widget(mSearchControl->widget, "limitResultsCheckResults")))) {
       g_strlcat(status->constantString,  g_strdup_printf(_("/Max hits limit:%d"), tmpLimit), MAX_FILENAME_STRING);
  }
  else {
       g_strlcat(status->constantString,  g_strdup_printf(_("/No Max hits limit")), MAX_FILENAME_STRING);
  }
  g_mutex_lock(&mutex_Control);
  stopSearch = mSearchControl->cancelSearch;
  g_mutex_unlock(&mutex_Control);
  if (stopSearch == TRUE) {
    g_strlcat(status->constantString, _(" [cancelled]"), MAX_FILENAME_STRING);
  }

  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (lookup_widget(mSearchControl->widget, "dosExpressionRadioFile"))))
    {
        g_strlcat(status->constantString, _("-research mode with jokers(DOS like)"), MAX_FILENAME_STRING);
       
    }
  else g_strlcat(status->constantString, _("-research mode with RegEx"), MAX_FILENAME_STRING);
   
  gtk_statusbar_pop(statusbar, STATUSBAR_CONTEXT_ID(statusbar));
  gtk_statusbar_push(statusbar, STATUSBAR_CONTEXT_ID(statusbar),
                     status->constantString);
  gdk_threads_leave ();
}


/*
 * POSIX threaded: phase one helper function.
 * Provides min/max modified-date and file-size checking, if selected by user.
 * Returns TRUE if the file passes all criteria set by user.
 */
gboolean statMatchPhase(const gchar *tmpFullFileName, searchControl *mSearchControl)
{
  struct stat buf;
    
  if ((mSearchControl->flags & SEARCH_SKIP_LINK_FILES) == 0) {
    stat(tmpFullFileName, &buf);
  } else {
    lstat(tmpFullFileName, &buf);
  }
  
  if ((mSearchControl->flags & SEARCH_MORETHAN_SET) != 0) {
      if (buf.st_size <= mSearchControl->moreThan) {
          return FALSE;
      }      
  }
  
  if ((mSearchControl->flags & SEARCH_LESSTHAN_SET) != 0) {
      if (buf.st_size >= mSearchControl->lessThan) {
          return FALSE;
      }      
  }
  
  if ((mSearchControl->flags & SEARCH_AFTER_SET) != 0) {
      if (difftime(buf.st_mtime, mSearchControl->after) <=0) {
          return FALSE;
      }
  }
  
  if ((mSearchControl->flags & SEARCH_BEFORE_SET) != 0) {
      if (difftime(mSearchControl->before, buf.st_mtime) <=0) {
          return FALSE;
      }      
  }
  return TRUE;
}


/*
 * POSIX threaded: phase two helper function.
 * Updates the Gtk GUI with match results for a full contents search.
 */
void displayMatch(searchControl *mSearchControl, searchData *mSearchData)
{
  GdkPixbuf    *pixbuf;
  textMatch *newMatch  = GET_LAST_PTR(mSearchData->textMatchArray);
  gchar *tmpStr = g_strdup_printf ("%d", (gint) newMatch->matchCount);

  pixbuf = get_icon_for_display(newMatch->pFileType);
  gdk_threads_enter ();
  g_assert(mSearchData->store != NULL);
  g_assert(mSearchData->iter != NULL);
//  g_assert(VALID_ITER (mSearchData->iter, GTK_LIST_STORE(mSearchData->store)));
  gtk_list_store_append (mSearchData->store, mSearchData->iter);
  gtk_list_store_set (mSearchData->store, mSearchData->iter,
                      ICON_COLUMN, GDK_PIXBUF(pixbuf ),/* Luc A, 1 janv 2018 */
                      FULL_FILENAME_COLUMN, newMatch->pFullName,
                      FILENAME_COLUMN, newMatch->pFileName,
                      LOCATION_COLUMN, newMatch->pLocation,
                      SIZE_COLUMN, newMatch->pFileSize,
                      INT_SIZE_COLUMN, newMatch->fileSize,
                      MATCHES_COUNT_COLUMN, newMatch->matchCount,
                      MATCH_INDEX_COLUMN, newMatch->matchIndex,
                      MODIFIED_COLUMN, newMatch->pMDate,
                      INT_MODIFIED_COLUMN, newMatch->mDate,
                      TYPE_COLUMN, newMatch->pShortFileType,
                      MATCHES_COUNT_STRING_COLUMN, tmpStr,
                      -1);
  gdk_threads_leave ();
  g_free(tmpStr);
  if (pixbuf!=NULL)
      g_object_unref(G_OBJECT(pixbuf));/* once loaded, the GdkPixbuf must be derefenced, cf : https://en.wikibooks.org/wiki/GTK%2B_By_Example/Tree_View/Tree_Models#Retrieving_Row_Data */
  return;
}


/*
 * POSIX threaded: phase one helper function.
 * Updates the Gtk GUI with match results for just the filename matches.
 */
void displayQuickMatch(searchControl *mSearchControl, searchData *mSearchData)
{
  const gchar *tmpStr = g_object_get_data(G_OBJECT(mainWindowApp), "notApplicable");
  GdkPixbuf    *pixbuf;
  textMatch *newMatch = g_malloc(sizeof(textMatch));
  g_ptr_array_add(mSearchData->textMatchArray, newMatch);
  newMatch->pFullName = GET_LAST_PTR(mSearchData->fullNameArray);
  newMatch->pFileName = GET_LAST_PTR(mSearchData->fileNameArray);
  newMatch->pLocation = GET_LAST_PTR(mSearchData->pLocationArray);
  
  getModified(mSearchData, newMatch);
  getFileType(mSearchData, newMatch);
  getFileSize(mSearchData, newMatch);
  getLength(mSearchData, newMatch);
  
  pixbuf = get_icon_for_display(newMatch->pFileType);
  gdk_threads_enter ();
  g_assert(mSearchData->store != NULL);
  g_assert(mSearchData->iter != NULL);
//  g_assert(VALID_ITER (mSearchData->iter, GTK_LIST_STORE(mSearchData->store)));
  gtk_list_store_append (mSearchData->store, mSearchData->iter);
  gtk_list_store_set (mSearchData->store, mSearchData->iter,
                      ICON_COLUMN, GDK_PIXBUF(pixbuf),/* Luc A, 1 janv 2018 */
                      FULL_FILENAME_COLUMN, newMatch->pFullName,
                      FILENAME_COLUMN, newMatch->pFileName,
                      LOCATION_COLUMN, newMatch->pLocation,
                      SIZE_COLUMN, newMatch->pFileSize,
                      INT_SIZE_COLUMN, newMatch->fileSize,
                      MODIFIED_COLUMN, newMatch->pMDate,
                      INT_MODIFIED_COLUMN, newMatch->mDate,
                      TYPE_COLUMN, newMatch->pShortFileType,
                      MATCHES_COUNT_STRING_COLUMN, tmpStr,
                      -1);
  if (pixbuf!=NULL) 
      g_object_unref(G_OBJECT(pixbuf));
  gdk_threads_leave ();
  return;
}


/*
 * Convert from GDate to struct tm format
 */
void setTimeFromDate(struct tm *tptr, GDate *date)
{
  tptr->tm_hour = 0; /* Hours */
  tptr->tm_isdst = 0; /* Is daylight saving enabled */
  tptr->tm_mday = date->day;
  tptr->tm_min = 0; /* Minutes */
  tptr->tm_mon = (date->month - 1); /* Month : 0=Jan */
  tptr->tm_sec = 0; /* Seconds */
  tptr->tm_wday = 0; /* Day of the week: 0=sun */
  tptr->tm_yday = 0; /* Day of the year: 0=NA */
  tptr->tm_year = (date->year - 1900); /* Year : 0=1900 AD */
}


/*************************************************************
 *  Constructors and destructors for search data
 *************************************************************/
/*
 * Constructs and initialises the master search data structure.
 * This data is used to store all match results during the search process.
 */
void createSearchData(GObject *object, const gchar *dataName)
{
  searchData *mSearchData;
  
  /* Create pointer arrays */
  mSearchData = g_malloc(sizeof(searchData));
  
  mSearchData->pLocationArray = g_ptr_array_new(); /* Only pointers to baseDirArray */
  mSearchData->textMatchArray = g_ptr_array_new();
  mSearchData->fileNameArray = g_ptr_array_new();
  mSearchData->fullNameArray = g_ptr_array_new();
  mSearchData->lineMatchArray = g_ptr_array_new();
  
  /* Create string chunks */
  mSearchData->locationChunk = g_string_chunk_new(MAX_FILENAME_STRING + 1);
  mSearchData->fileSizeChunk = g_string_chunk_new(MAX_FILENAME_STRING + 1);
  mSearchData->mDateChunk = g_string_chunk_new(MAX_FILENAME_STRING + 1);
  mSearchData->fileTypeChunk = g_string_chunk_new(MAX_FILENAME_STRING + 1);
  mSearchData->shortfileTypeChunk = g_string_chunk_new(MAX_FILENAME_STRING + 1);/* Luc A march 2018 */
  mSearchData->textMatchChunk = g_string_chunk_new(MAX_FILENAME_STRING + 1);

//  mSearchData->store = NULL;
  mSearchData->iter = g_malloc(sizeof(GtkTreeIter));
  
  /* Attach the data to the G_OBJECT */
  g_object_set_data_full(object, dataName, mSearchData, destroySearchData);
}


/*
 * Destroys the master search data structure.
 * This data is used to store all match results during the search process.
 * Automatically called when data object is destroyed, or re-created.
 */
void destroySearchData(gpointer data)
{
  searchData *mSearchData = data;

  g_free(mSearchData->iter);
  
  /* Destroy string chunks */
  g_string_chunk_free(mSearchData->locationChunk);
  g_string_chunk_free(mSearchData->fileSizeChunk);
  g_string_chunk_free(mSearchData->mDateChunk);
  g_string_chunk_free(mSearchData->fileTypeChunk);
  g_string_chunk_free(mSearchData->textMatchChunk);

  /* Destroy pointed to data */
  g_ptr_array_foreach(mSearchData->textMatchArray, ptr_array_free_cb, GINT_TO_POINTER(1));
  g_ptr_array_foreach(mSearchData->fileNameArray, ptr_array_free_cb, NULL);
  g_ptr_array_foreach(mSearchData->fullNameArray, ptr_array_free_cb, NULL);
  g_ptr_array_foreach(mSearchData->lineMatchArray, ptr_array_free_cb, NULL);

  /* Destroy pointer arrays, plus malloc'ed arrays */
  g_ptr_array_free(mSearchData->pLocationArray, TRUE); 
  g_ptr_array_free(mSearchData->textMatchArray, TRUE);
  g_ptr_array_free(mSearchData->fileNameArray, TRUE);
  g_ptr_array_free(mSearchData->fullNameArray, TRUE);
  g_ptr_array_free(mSearchData->lineMatchArray, TRUE);

  /* And last of all, remove the structure and NULL the pointers! */
//  mSearchData->iter = NULL;
  g_free(mSearchData);
  //mSearchData->store = NULL; /* Clear the pointer to it.. */
}


/*
 * Constructs and initialises the master search control structure.
 * This data is used to store pointers to the data stored within
 * the search criteria at the instance that search is pressed.
 * No custom destructor required as g_free is adequate.
 */
void createSearchControl(GObject *object, const gchar *dataName)
{
  searchControl *prevSearchControl = g_object_get_data(object, dataName);
  searchControl *mSearchControl;

  if (prevSearchControl != NULL) {
    g_object_set_data(object, dataName, NULL); /* Try to force clean-up of the data prior to recreation */
  }

  /* Create pointer arrays*/
  mSearchControl = g_malloc(sizeof(searchControl));
  mSearchControl->flags = 0;
  mSearchControl->textSearchFlags = 0;
  mSearchControl->fileSearchFlags = 0;
  mSearchControl->limitContentResults = 0;
  mSearchControl->numExtraLines = 0;
  mSearchControl->limitResults = 0;
  mSearchControl->folderDepth = 0;

  mSearchControl->textSearchRegEx = NULL;
  mSearchControl->fileSearchRegEx = NULL;
  mSearchControl->startingFolder = NULL;

  /* Attach the data to the G_OBJECT */
  g_object_set_data_full(object, dataName, mSearchControl, destroySearchControl);
}


/*
 * Constructs and initialises the master search control structure.
 * This data is used to store pointers to the data stored within
 * the search criteria at the instance that search is pressed.
 * No custom destructor required as g_free is adequate.
 */
void destroySearchControl(gpointer data)
{
  searchControl *mSearchControl = data;

  /* Free the text strings (if present) */
  if (mSearchControl->textSearchRegEx != NULL) {
    g_free(mSearchControl->textSearchRegEx);
  }
  if (mSearchControl->fileSearchRegEx != NULL) {
    g_free(mSearchControl->fileSearchRegEx);
  }
  if (mSearchControl->startingFolder != NULL) {
    g_free(mSearchControl->startingFolder);
  }

  /* Finally free the remaining structure */
  g_free(mSearchControl);
}


/*
 * Constructs and initialises the status bar structure.
 * This data is used to hold the status bar, and its associated text.
 */
void createStatusbarData(GObject *object, const gchar *dataName)
{
  statusbarData *status;

  status = g_malloc(sizeof(statusbarData));
  status->statusbarChunk = g_string_chunk_new(MAX_FILENAME_STRING + 1);

  /* Attach the data to the G_OBJECT */
  g_object_set_data_full(object, dataName, status, destroyStatusbarData);
}


/*
 * Destroys the status bar structure.
 * This data is used to hold the status bar, and its associated text.
 * Automatically called by when object is destroyed, or re-created.
 */
void destroyStatusbarData(gpointer data)
{
  statusbarData *status = data;
  g_string_chunk_free(status->statusbarChunk);
  g_free(status);
}


/*
 * Replacement pointer array free as it does not seem to really g_free the data being pointed to
 */
void ptr_array_free_cb(gpointer data, gpointer user_data)
{
  g_free(data);
}
 
