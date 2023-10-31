#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif
#include <stdlib.h>

#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>

#include "interface.h"
#include "support.h"
#include "search.h"
#include "lgpl.h" /* Lesser GPL license*/
#include "savestate.h" /* Allows config.ini to save/restore state*/
#include "regexwizard.h" /* Add support for regular expression wizard */
#include "systemio.h" /* System stuff, file import and export code */
#include "misc.h" /* Everything else */
#include "systemio.h" /* Add structure */
#include "callbacks.h"


/* Top level definitions */
GtkWidget *testRegExDialog1 = NULL;
GtkWidget *fileRegexWizard = NULL;
GtkWidget *contextRegexWizard = NULL;

void
on_window1_destroy                     (GtkWidget       *object,
                                        gpointer         user_data)
{
  gtk_main_quit();
}


void
on_open_criteria1_activate             (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  importCriteria(GTK_WIDGET(menuitem));
}


void
on_save_criteria1_activate             (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  exportCriteria(GTK_WIDGET(menuitem));
}


void
on_save_results1_activate              (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  saveResults(GTK_WIDGET(menuitem));
}


void
on_print1_activate                     (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}


void
on_print_preview1_activate             (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}


void
on_print_setup1_activate               (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}


void
on_quit1_activate                      (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  GObject *window1 = G_OBJECT(lookup_widget(GTK_WIDGET(menuitem), "window1"));
   gtk_widget_destroy(GTK_WIDGET(window1));
}


void
on_word_wrap1_activate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    gboolean setWordWrap;
    GtkTextView *textBox;
    if (getResultsViewHorizontal(GTK_WIDGET(menuitem))) {
      textBox = GTK_TEXT_VIEW(lookup_widget(GTK_WIDGET(menuitem), "textview1"));
    } else {
      textBox = GTK_TEXT_VIEW(lookup_widget(GTK_WIDGET(menuitem), "textview4"));
    }
    
    setWordWrap = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(menuitem));
    if(setWordWrap)
        gtk_text_view_set_wrap_mode(textBox, GTK_WRAP_WORD);
          else gtk_text_view_set_wrap_mode(textBox, GTK_WRAP_NONE);
    gtk_text_view_set_justification( GTK_TEXT_VIEW(textBox), GTK_JUSTIFY_LEFT );
}


void
on_set_font1_activate                  (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    gchar *newFont;
    GtkWidget *textView1, *textView4;
    GtkWidget *textView;
    if (getResultsViewHorizontal(GTK_WIDGET(menuitem))) {
      textView = lookup_widget(GTK_WIDGET(menuitem), "textview1");
    } else {
      textView = lookup_widget(GTK_WIDGET(menuitem), "textview4");
    }
    textView1 = lookup_widget(GTK_WIDGET(menuitem), "textview1");
    textView4 = lookup_widget(GTK_WIDGET(menuitem), "textview4");
    
    GtkFontSelectionDialog *dialog = create_fontSelectionDialog();
    PangoContext* context = gtk_widget_get_pango_context  (textView);
    PangoFontDescription *desc = pango_context_get_font_description(context);    
    newFont = pango_font_description_to_string(desc);

    gtk_font_chooser_set_font (GTK_FONT_CHOOSER(dialog ),newFont);

    if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK) {
      newFont = gtk_font_chooser_get_font(GTK_FONT_CHOOSER(dialog ));
      if (newFont != NULL) { 
        desc = pango_font_description_from_string (newFont);
        if (desc != NULL) {
          gtk_widget_modify_font (GTK_WIDGET(textView1), desc);
          gtk_widget_modify_font (GTK_WIDGET(textView4), desc);
          pango_font_description_free(desc);
        }
        g_free(newFont);
      }
    }
    gtk_widget_destroy(GTK_WIDGET(dialog));
}


void
on_set_highligting_colour1_activate    (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    GdkRGBA color, *cp;
    GtkTextView *textView1, *textView4;

    textView1 = GTK_TEXT_VIEW(lookup_widget(GTK_WIDGET(menuitem), "textview1"));
    textView4 = GTK_TEXT_VIEW(lookup_widget(GTK_WIDGET(menuitem), "textview4"));
    
    GtkTextBuffer* textBuf1 = gtk_text_view_get_buffer (textView1);
    GtkTextTagTable* tagTable1 = gtk_text_buffer_get_tag_table(textBuf1);
    GtkTextTag* tag1 = gtk_text_tag_table_lookup(tagTable1, "word_highlight");
    GtkTextBuffer* textBuf4 = gtk_text_view_get_buffer (textView4);
    GtkTextTagTable* tagTable4 = gtk_text_buffer_get_tag_table(textBuf4);
    GtkTextTag* tag4 = gtk_text_tag_table_lookup(tagTable4, "word_highlight");

    GtkColorChooserDialog *dialog = create_highlightColourDialog();

    g_assert(textView1 != NULL);
    g_assert(textBuf1 != NULL);
    g_assert(tagTable1 != NULL);
    g_assert(tag1 != NULL);
    g_assert(dialog != NULL);
    
    if (getResultsViewHorizontal(GTK_WIDGET(menuitem))) {
      g_object_get( G_OBJECT(tag1), "background-rgba", &cp, NULL);
    } else {
      g_object_get( G_OBJECT(tag4), "background-rgba", &cp, NULL);
    }
    
    gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(dialog), cp);
    gdk_rgba_free (cp);
    if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK) {
        gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(dialog), &color);
        g_object_set( G_OBJECT(tag1), "background-rgba", &color, NULL);
        g_object_set( G_OBJECT(tag4), "background-rgba", &color, NULL);
    }
    gtk_widget_destroy(dialog);
}


void
on_cl_ear_history1_activate            (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    GtkToggleButton *check;
    GtkComboBox *combo;
    gchar *activeText;
    GtkEntry *entry;
    GtkWidget *dialog = create_clearSearchHistoryDialog();

    if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK) {
        /* Clear file names? */
        check = GTK_TOGGLE_BUTTON(lookup_widget(dialog, "clearFileNamesCheck"));
        if(gtk_toggle_button_get_active(check)) {
          clearComboBox2(lookup_widget(GTK_WIDGET(menuitem), "fileName"));
        }

        /* Clear containing text? */
        check = GTK_TOGGLE_BUTTON(lookup_widget(dialog, "clearContainingTextCheck"));
        if(gtk_toggle_button_get_active(check)) {
            clearComboBox(lookup_widget(GTK_WIDGET(menuitem), "containingText"));
        }

        /* Clear look in? */
        check = GTK_TOGGLE_BUTTON(lookup_widget(dialog, "clearLookInCheck"));
        if(gtk_toggle_button_get_active(check)) {
            clearComboBox(lookup_widget(GTK_WIDGET(menuitem), "lookIn"));

        }

        /* Clear size/modified? */
        check = GTK_TOGGLE_BUTTON(lookup_widget(dialog, "resetSizeModifiedCheck"));
        if(gtk_toggle_button_get_active(check)) {
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(lookup_widget(GTK_WIDGET(menuitem), "lessThanCheck")), FALSE);
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(lookup_widget(GTK_WIDGET(menuitem), "moreThanCheck")), FALSE);
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(lookup_widget(GTK_WIDGET(menuitem), "afterCheck")), FALSE);
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(lookup_widget(GTK_WIDGET(menuitem), "beforeCheck")), FALSE);

            entry = GTK_ENTRY(lookup_widget(GTK_WIDGET(menuitem), "lessThanEntry"));
            gtk_entry_set_text(entry, "");
            entry = GTK_ENTRY(lookup_widget(GTK_WIDGET(menuitem), "moreThanEntry"));
            gtk_entry_set_text(entry, "");
            entry = GTK_ENTRY(lookup_widget(GTK_WIDGET(menuitem), "afterEntry"));
            gtk_entry_set_text(entry, "");
            entry = GTK_ENTRY(lookup_widget(GTK_WIDGET(menuitem), "beforeEntry"));
            gtk_entry_set_text(entry, "");
        }
    }
    gtk_widget_destroy(dialog);
}


void
on_delete1_activate                    (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  deleteFile (GTK_WIDGET(menuitem));
}


void
on_copy2_activate                      (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  copyFile (GTK_WIDGET(menuitem));
}




void
on_status_bar1_activate                (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  GtkWidget *widget = lookup_widget(GTK_WIDGET(menuitem), "hbox41"); /* Contains status plus progress bar */
  if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(menuitem))) {
    gtk_widget_show(widget);
  } else {
    gtk_widget_hide(widget);
  }
}


void
on_file_name1_activate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(menuitem))) {
    columnClick(GTK_WIDGET(menuitem), FILENAME_COLUMN);
  }
}


void
on_location1_activate                  (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(menuitem))) {
    columnClick(GTK_WIDGET(menuitem), LOCATION_COLUMN);
  }
}


void
on_size1_activate                      (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(menuitem))) {
    columnClick(GTK_WIDGET(menuitem), INT_SIZE_COLUMN);
  }
}


void
on_type1_activate                      (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(menuitem))) {
    columnClick(GTK_WIDGET(menuitem), TYPE_COLUMN);
  }
}


void
on_modified1_activate                  (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(menuitem))) {
    columnClick(GTK_WIDGET(menuitem), INT_MODIFIED_COLUMN);
  }
}


void
on_search1_activate                    (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}


void
on_configuration1_activate             (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  GtkWidget *dialog = create_configDialog();

  realize_configDialog(dialog);

  switch (gtk_dialog_run(GTK_DIALOG(dialog))) {
  case GTK_RESPONSE_OK:
    unrealize_configDialog(dialog);
    break;
  default:
    break;
  }
  gtk_widget_destroy(dialog);
}


void
on_test1_activate                      (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}


void
on_reg_expression1_activate            (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  if (testRegExDialog1 == NULL) {
    testRegExDialog1 = create_testRegExDialog();
    gtk_widget_show(testRegExDialog1);
  } else {
    gtk_widget_grab_focus(lookup_widget(testRegExDialog1, "testEntry"));
  }
}


void
on_1_search1_activate                  (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

}


void
on_contents1_activate                  (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  SMsyscall(_("http://searchmonkey.embeddediq.com/index.php/contribute"), BROWSER_LIST);/* must ne changed, Luc A feb 2018 */
}


void
on_support1_activate                   (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  SMsyscall(_("http://searchmonkey.embeddediq.com/index.php/support"), BROWSER_LIST);
}


void
on_about1_activate                     (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  GtkWidget *aboutDialog = create_aboutSearchmonkey();
  GtkWidget *tmpWidget;
  gchar *tmpString;
  PangoAttrList *list;
  PangoAttribute *attr;
  
  gtk_dialog_run(GTK_DIALOG (aboutDialog));  
  gtk_widget_destroy(GTK_DIALOG (aboutDialog));
  return;
}
  /* Set searchmonkey version text and font size */
//  tmpWidget = lookup_widget(aboutDialog, "aboutVersion");
 // tmpString = g_strdup_printf(_("searchmonkey %s"), VERSION);/* defined in "configure" line 1644 - luc A 3 janv 2018 */
 // gtk_label_set_text(GTK_LABEL(tmpWidget), tmpString);
 // g_free(tmpString);
//  list = pango_attr_list_new(); /* Create list with 1 reference */
 // attr = pango_attr_scale_new(PANGO_SCALE_X_LARGE);
 // pango_attr_list_change(list, attr); /* pango_attr_list_insert */
 // gtk_label_set_attributes(GTK_LABEL(tmpWidget), list);
 // pango_attr_list_unref(list); /* Destroy 1 attribute, plus list */

  /* Start widget */
 // gtk_widget_show(aboutDialog);



void
on_printResult_clicked                 (GtkButton       *button,
                                        gpointer         user_data)
{
  //    g_print("Creating XML data...\n");
  //    g_print("Printing XML data...\n");
}


void
on_Question_clicked                    (GtkButton       *button,
                                        gpointer         user_data)
{
  SMsyscall(_("http://sourceforge.net/forum/?group_id=175143"), BROWSER_LIST);
}


void
on_containingText_changed              (GtkComboBox     *combobox,
                                        gpointer         user_data)
{
  GtkToggleButton *checkBox;

 
  checkBox = GTK_TOGGLE_BUTTON(lookup_widget(GTK_WIDGET(combobox), "containingTextCheck"));

  gchar *test = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT(combobox));

  if (test == NULL){
    gtk_toggle_button_set_active(checkBox, FALSE);
    return;
  } else {
    if (*test == '\0') {
      gtk_toggle_button_set_active(checkBox, FALSE);
    } else {
      gtk_toggle_button_set_active(checkBox, TRUE);
    }
    g_free(test);
  }
}


void
on_folderSelector_clicked              (GtkButton       *button,
                                        gpointer         user_data)
{
  GtkWidget *dialog;
  GtkComboBox *fileWidget;
  gint result;
  gchar *currentFolderName = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT(lookup_widget(GTK_WIDGET(button), "lookIn")));
  
  dialog = create_folderChooserDialog();
  gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), currentFolderName);
  g_free(currentFolderName);

  if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK) {
    gchar *filename;
    
    filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
    fileWidget = GTK_COMBO_BOX(lookup_widget(GTK_WIDGET(button), "lookIn"));
    addUniqueRow(GTK_WIDGET(fileWidget), filename);
    gtk_widget_set_tooltip_text (GTK_WIDGET(lookup_widget(GTK_WIDGET(button), "lookIn")),filename ); 
    g_free (filename);
  }
  gtk_widget_destroy (dialog);
}


void
on_lessThanCheck_toggled               (GtkToggleButton *togglebutton,
                                        gpointer         user_data)
{
  GtkWidget *pTextBox = lookup_widget(GTK_WIDGET(togglebutton), "lessThanEntry");
  gtk_widget_set_sensitive(pTextBox, gtk_toggle_button_get_active(togglebutton));
}


void
on_beforeCheck_toggled                 (GtkToggleButton *togglebutton,
                                        gpointer         user_data)
{
  GtkWidget *pTextBox = lookup_widget(GTK_WIDGET(togglebutton), "beforeEntry");
  gtk_widget_set_sensitive(pTextBox, gtk_toggle_button_get_active(togglebutton));
  gtk_widget_set_sensitive(lookup_widget(GTK_WIDGET(togglebutton), "beforeCalendarBtn"), gtk_toggle_button_get_active(togglebutton));
}


void
on_moreThanCheck_toggled               (GtkToggleButton *togglebutton,
                                        gpointer         user_data)
{
  GtkWidget *pTextBox = lookup_widget(GTK_WIDGET(togglebutton), "moreThanEntry");
  gtk_widget_set_sensitive(pTextBox, gtk_toggle_button_get_active(togglebutton));
}


void
on_afterCheck_toggled                  (GtkToggleButton *togglebutton,
                                        gpointer         user_data)
{
  GtkWidget *pTextBox = lookup_widget(GTK_WIDGET(togglebutton), "afterEntry");
  gtk_widget_set_sensitive(pTextBox, gtk_toggle_button_get_active(togglebutton));
  gtk_widget_set_sensitive(lookup_widget(GTK_WIDGET(togglebutton), "afterCalendarBtn"), gtk_toggle_button_get_active(togglebutton));
}


gboolean
on_treeview1_button_press_event        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data)
{
  GtkTreeView *treeview = GTK_TREE_VIEW(widget);
  GtkTreePath *path;
  GtkTreeViewDropPosition *pos;


  GtkTreeSelection *selection = gtk_tree_view_get_selection (treeview);

  if(selection==NULL)
     return FALSE;
  /* Capture right button click */
  if ((event->button == 3) && (event->type == GDK_BUTTON_PRESS) && (gtk_tree_selection_count_selected_rows(selection)==1 ) ) 
   {
    if (gtk_tree_view_get_path_at_pos (treeview,
                                       event->x, event->y,
                                       &path, NULL, NULL, NULL)) 
     {
       // gtk_tree_selection_unselect_all(selection); /* added by Luc A., 28 dec 2017 */
       gtk_tree_selection_select_path (selection, path);
       if (path!=NULL) 
         {
          gtk_tree_path_free(path);
         }/* if path!=NULL */
       do_popup_menu(widget, event);
       return TRUE;
     }/* if test path at pos OK */   
    else return FALSE;
   }/* endif right-click */ 
 /* capture double-click */
 if ((event->button == 1) && (event->type == GDK_2BUTTON_PRESS) && (gtk_tree_selection_count_selected_rows(selection)==1 )) 
   {
     if (gtk_tree_view_get_path_at_pos (treeview,event->x, event->y,
                                    &path, NULL, NULL, NULL)) 
      {
          gchar *fullFileName = getFullFileName(treeview, FULL_FILENAME_COLUMN);
          if (fullFileName != NULL) 
            {
              SMsyscall(fullFileName, TEXTEDITOR_LIST);
              g_free(fullFileName);
            }
          return TRUE;
      }/* endif at pos */
    }/* endif double-click */
   
 /* capture simple-left-click, i.e. select a row */
 if ((event->button == 1) && (event->type == GDK_BUTTON_PRESS)  && (gtk_tree_selection_count_selected_rows(selection)==1 )) 
   {
     if (gtk_tree_view_get_path_at_pos (treeview,event->x, event->y,
                                    &path, NULL, NULL, NULL)) 
      {
            gtk_tree_selection_unselect_all(selection); /* ajout */
            if (gtk_tree_selection_count_selected_rows(selection)==1)
             {
               gtk_tree_selection_select_path (selection, path);
               if (path!=NULL) 
                {
                 gtk_tree_path_free(path);                 
                }    
              return TRUE;
             }
      }
   }/* endif left-click */
 return FALSE;
}


gboolean
on_treeview1_popup_menu                (GtkWidget       *widget,
                                        gpointer         user_data)
{
  do_popup_menu(widget, NULL);
  return FALSE;
}


void
on_open1_activate                      (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  GtkTreeView *treeView;
  gchar *fullFileName;
  
  if (getResultsViewHorizontal(GTK_WIDGET(menuitem))) {
    treeView = GTK_TREE_VIEW(lookup_widget(GTK_WIDGET(menuitem), "treeview1"));
  } else {
    treeView = GTK_TREE_VIEW(lookup_widget(GTK_WIDGET(menuitem), "treeview2"));
  }
  fullFileName = getFullFileName(treeView, FULL_FILENAME_COLUMN);

  if (fullFileName != NULL) {
    SMsyscall(fullFileName, TEXTEDITOR_LIST);
    g_free(fullFileName);
  }
  
  detachMenu(menuitem);
}


void
on_copy3_activate                      (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  copyFile(GTK_WIDGET(menuitem));    
  detachMenu(menuitem);
}


void
on_delete2_activate                    (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  deleteFile (GTK_WIDGET(menuitem));
  detachMenu(menuitem);
}


void
on_explore1_activate                   (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  GtkTreeView *treeView;
  gchar *location;
  
  if (getResultsViewHorizontal(GTK_WIDGET(menuitem))) {
    treeView = GTK_TREE_VIEW(lookup_widget(GTK_WIDGET(menuitem), "treeview1"));
  } else {
    treeView = GTK_TREE_VIEW(lookup_widget(GTK_WIDGET(menuitem), "treeview2"));
  }
  location = getFullFileName(treeView, LOCATION_COLUMN);

  if (location != NULL) {
    SMsyscall(location, FILEEXPLORER_LIST);
    g_free(location);
  }
  
  detachMenu(menuitem);
}


void
on_cancel1_activate                    (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  detachMenu(menuitem);
}


void
on_testRegExDialog_response            (GtkDialog       *dialog,
                                        gint             response_id,
                                        gpointer         user_data)
{
    switch (response_id) {
        case GTK_RESPONSE_APPLY:
            refreshTestResults(GTK_WIDGET(dialog));
            break;
        default:
            testRegExDialog1 = NULL;
            gtk_widget_destroy(GTK_WIDGET(dialog));
            break;
    }
}


void
on_SampleTextView_realize              (GtkWidget       *widget,
                                        gpointer         user_data)
{
  GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW(widget));
  GtkTextTag *tag;
  
  tag = gtk_text_buffer_create_tag (buffer, "word_highlight",
                                    "foreground", "black","background", "lightBlue",
                                    NULL);
#ifdef NOT_YET
  tag = gtk_text_buffer_create_tag (buffer, "word_highlight1",
                                    "foreground", "lightBlue",
                                    NULL);
#endif
}


void
on_expWizard_response                  (GtkDialog       *dialog,
                                        gint             response_id,
                                        gpointer         user_data)
{
    gint typeRegex = (intptr_t)g_object_get_data(G_OBJECT(dialog), "regexType");
    GtkEntry *output = GTK_ENTRY(lookup_widget(GTK_WIDGET(dialog), "resultExp"));/* resulExp = name of GtkEntry for the final RegEx formula */
    gchar *finalRegex;
    GtkComboBox *retCombo;/* contains a pointer on main Window, for GtkCombo for files OR GtkCombo for containing text */
    
    if (typeRegex == FILE_REGEX_TYPE) {
        retCombo = GTK_COMBO_BOX(lookup_widget(mainWindowApp, "fileName"));/* filename = GtkWidget, field of the combo in main window */
    } else if (typeRegex == CONTEXT_REGEX_TYPE) {
        retCombo = GTK_COMBO_BOX(lookup_widget(mainWindowApp, "containingText"));
    } else {
        g_print (_("Internal Error! Unable to find calling wizard type!"));
        gtk_widget_destroy(GTK_WIDGET(dialog));
        return;
    }
    
    switch (response_id) {
        case GTK_RESPONSE_HELP:
            SMsyscall(_("http://searchmonkey.sourceforge.net/index.php/Regular_expression_builder"), BROWSER_LIST);
            break;
        case GTK_RESPONSE_OK:
            finalRegex = (gchar *)gtk_entry_get_text(output);/* read text in Wizard dialog, resulting formula */
            if (*finalRegex != '\0') {
printf("tentative regex\n");
                addUniqueRow(GTK_WIDGET(retCombo), finalRegex);/* modify combobox in main window */
            }
            /* Do not break here! We want the widget to be destroyed! */

        default:
            if (typeRegex == FILE_REGEX_TYPE) {
                fileRegexWizard = NULL;
            } else if (typeRegex == CONTEXT_REGEX_TYPE) {
                contextRegexWizard = NULL;
            }
            gtk_widget_destroy(GTK_WIDGET(dialog));
            break;
    }
}


void
on_startType_changed                   (GtkComboBox     *combobox,
                                        gpointer         user_data)
{
    GtkWidget *entry = lookup_widget(GTK_WIDGET(combobox), "startEntry");
    GtkWidget *repeat = lookup_widget(GTK_WIDGET(combobox), "startOccurance");
    updateTypeChangeEntry(combobox, entry, repeat);
    updateRegExWizard(GTK_WIDGET(combobox));
}


void
on_startEntry_changed                  (GtkEditable     *editable,
                                        gpointer         user_data)
{
    updateRegExWizard(GTK_WIDGET(editable));
}


void
on_startOccurance_changed              (GtkComboBox     *combobox,
                                        gpointer         user_data)
{
    updateRegExWizard(GTK_WIDGET(combobox));
}


void
on_midType_changed                     (GtkComboBox     *combobox,
                                        gpointer         user_data)
{
    GtkWidget *entry = lookup_widget(GTK_WIDGET(combobox), "midEntry");
    GtkWidget *repeat = lookup_widget(GTK_WIDGET(combobox), "midOccurance");

    updateTypeChangeEntry(combobox, entry, repeat);
    updateRegExWizard(GTK_WIDGET(combobox));
    
}


void
on_midEntry_changed                    (GtkEditable     *editable,
                                        gpointer         user_data)
{
    updateRegExWizard(GTK_WIDGET(editable));
}


void
on_midOccurance_changed                (GtkComboBox     *combobox,
                                        gpointer         user_data)
{
    updateRegExWizard(GTK_WIDGET(combobox));
}


void
on_endType_changed                     (GtkComboBox     *combobox,
                                        gpointer         user_data)
{
    GtkWidget *entry = lookup_widget(GTK_WIDGET(combobox), "endEntry");
    GtkWidget *repeat = lookup_widget(GTK_WIDGET(combobox), "endOccurance");
    updateTypeChangeEntry(combobox, entry, repeat);
    updateRegExWizard(GTK_WIDGET(combobox));
}


void
on_endEntry_changed                    (GtkEditable     *editable,
                                        gpointer         user_data)
{
    updateRegExWizard(GTK_WIDGET(editable));
}


void
on_endOccurance_changed                (GtkComboBox     *combobox,
                                        gpointer         user_data)
{
    updateRegExWizard(GTK_WIDGET(combobox));
}


void
on_regExpWizard1_clicked               (GtkButton       *button,
                                        gpointer         user_data)
{
    /* Create filename wizard */
    if (fileRegexWizard == NULL) {
        fileRegexWizard = create_expWizard();
        g_object_set_data(G_OBJECT(fileRegexWizard), "regexType", (gpointer)FILE_REGEX_TYPE); /* file type */
        gtk_widget_show(fileRegexWizard);
    } else {
        gtk_widget_grab_focus(lookup_widget(fileRegexWizard, "startEntry"));
    }
}


void
on_regExpWizard2_clicked               (GtkButton       *button,
                                        gpointer         user_data)
{
    /* Create filename wizard */
    if (contextRegexWizard == NULL) {
        contextRegexWizard = create_expWizard();
        g_object_set_data(G_OBJECT(contextRegexWizard), "regexType", (gpointer)CONTEXT_REGEX_TYPE); /* file type */
        gtk_widget_show(contextRegexWizard);
    } else {
        gtk_widget_grab_focus(lookup_widget(contextRegexWizard, "startEntry"));
    }
}


void
on_convertRegex_toggled                (GtkToggleButton *togglebutton,
                                        gpointer         user_data)
{
    updateRegExWizard(GTK_WIDGET(togglebutton));
}


void
on_addMidContents_clicked              (GtkButton       *button,
                                        gpointer         user_data)
{
    GtkComboBox *type = GTK_COMBO_BOX(lookup_widget(GTK_WIDGET(button), "midType"));    
    GtkEntry *entry = GTK_ENTRY(lookup_widget(GTK_WIDGET(button), "midEntry"));
    GtkComboBox *repeat = GTK_COMBO_BOX(lookup_widget(GTK_WIDGET(button), "midOccurance"));
    GtkEntry *iterStr = GTK_ENTRY(lookup_widget(GTK_WIDGET(button), "midSelection"));
    
    appendTableRow(lookup_widget(GTK_WIDGET(button), "midTreeView"),
                   5,
                   gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT(type)),
                   gtk_entry_get_text(entry),
                   gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT(repeat)),
                   gtk_combo_box_get_active(type),
                   gtk_combo_box_get_active(repeat));
    gtk_combo_box_set_active(type, 0);
    gtk_entry_set_text(entry, "");
    gtk_combo_box_set_active(repeat, 0);
    gtk_entry_set_text(iterStr, "");
    gtk_widget_set_sensitive(lookup_widget(GTK_WIDGET(button), "deleteSelectedContents"), TRUE);
    gtk_widget_set_sensitive(lookup_widget(GTK_WIDGET(button), "modifiedSelectedContents"), TRUE);
}


void
on_modifiedSelectedContents_clicked    (GtkButton       *button,
                                        gpointer         user_data)
{
  GtkTreeView *treeview = GTK_TREE_VIEW(lookup_widget(GTK_WIDGET(button), "midTreeView"));
  GtkTreeSelection *selection = gtk_tree_view_get_selection (treeview);
  GtkTreeIter iter;
  GtkTreeModel *model;
  gint type = REGWIZ_NONE;
  gchar *entry = NULL;
  gint repeat = REGWIZ_NONE;
  gchar *iterStr;

  g_assert(treeview != NULL);
  g_assert(selection != NULL);
  
  if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
    g_assert(model != NULL);
    gtk_tree_model_get (model, &iter, REGEX_TYPE_INT_COLUMN, &type,
                        REGEX_ENTRY_COLUMN, &entry,
                        REGEX_REPEAT_INT_COLUMN, &repeat,
                        -1);
    
    gtk_combo_box_set_active(GTK_COMBO_BOX(lookup_widget(GTK_WIDGET(button), "midType")), type);
    gtk_entry_set_text(GTK_ENTRY(lookup_widget(GTK_WIDGET(button), "midEntry")), entry);
    gtk_combo_box_set_active(GTK_COMBO_BOX(lookup_widget(GTK_WIDGET(button), "midOccurance")), repeat);
    g_free (entry);

    iterStr = gtk_tree_model_get_string_from_iter (model, &iter);
    if (iterStr != NULL) {
      gtk_entry_set_text(GTK_ENTRY(lookup_widget(GTK_WIDGET(button), "midSelection")), iterStr);
      g_free(iterStr);
    }
    
  }
}


void
on_deleteSelectedContents_clicked      (GtkButton       *button,
                                        gpointer         user_data)
{
  GtkTreeView *treeview = GTK_TREE_VIEW(lookup_widget(GTK_WIDGET(button), "midTreeView"));
  GtkTreeSelection *selection = gtk_tree_view_get_selection (treeview);
  GtkTreeIter iter;
  GtkTreeModel *model;
  GtkComboBox *type = GTK_COMBO_BOX(lookup_widget(GTK_WIDGET(button), "midType"));    
  GtkEntry *entry = GTK_ENTRY(lookup_widget(GTK_WIDGET(button), "midEntry"));
  GtkEntry *iterC = GTK_ENTRY(lookup_widget(GTK_WIDGET(button), "midSelection"));
  GtkComboBox *repeat = GTK_COMBO_BOX(lookup_widget(GTK_WIDGET(button), "midOccurance"));
  
  if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
    gtk_list_store_remove(GTK_LIST_STORE(model), &iter);
    gtk_combo_box_set_active(type, 0);
    gtk_entry_set_text(entry, "");
    gtk_combo_box_set_active(repeat, 0);
    gtk_entry_set_text(iterC, "");
  }
  if (gtk_tree_model_iter_n_children (model, NULL) <= 0) {
    gtk_widget_set_sensitive(lookup_widget(GTK_WIDGET(button), "modifiedSelectedContents"), FALSE);
    gtk_widget_set_sensitive(lookup_widget(GTK_WIDGET(button), "deleteSelectedContents"), FALSE);
  }
  updateRegExWizard(GTK_WIDGET(button));
}

/* Regex Wizard : middle part of the expression */
void
on_midTreeView_realize                 (GtkWidget       *widget,
                                        gpointer         user_data)
{
    GtkTreeViewColumn *column;
    GtkCellRenderer *renderer = gtk_cell_renderer_text_new ();

    GtkListStore *store = gtk_list_store_new (REGEX_N_COLUMNS,/* number of total columns in Regex wizard- 1 janv 2018 Luc A*/                                            
                                              G_TYPE_STRING,
                                              G_TYPE_STRING,
                                              G_TYPE_STRING,
                                              G_TYPE_INT,
                                              G_TYPE_INT,
                                              G_TYPE_INT);
    gtk_tree_view_set_model (GTK_TREE_VIEW(widget), GTK_TREE_MODEL(store));
    g_object_unref (G_OBJECT (store));

    /* Create columns */
    column = gtk_tree_view_column_new_with_attributes (_("(Type)"), renderer,
                                                       "text", REGEX_TYPE_COLUMN,
                                                       NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (widget), column);

    column = gtk_tree_view_column_new_with_attributes (_("(Entry)"), renderer,
                                                       "text", REGEX_ENTRY_COLUMN,
                                                       NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (widget), column);

    column = gtk_tree_view_column_new_with_attributes (_("(Repeat)"), renderer,
                                                       "text", REGEX_REPEAT_COLUMN,
                                                       NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (widget), column);
}


void
on_updateSelectedContents_clicked      (GtkButton       *button,
                                        gpointer         user_data)
{
  GtkTreeView *treeview = GTK_TREE_VIEW(lookup_widget(GTK_WIDGET(button), "midTreeView"));
  GtkTreeIter iter;
  GtkTreeModel *model = gtk_tree_view_get_model (treeview);
  GtkComboBox *type = GTK_COMBO_BOX(lookup_widget(GTK_WIDGET(button), "midType"));    
  GtkEntry *entry = GTK_ENTRY(lookup_widget(GTK_WIDGET(button), "midEntry"));
  GtkEntry *iterC = GTK_ENTRY(lookup_widget(GTK_WIDGET(button), "midSelection"));
  GtkComboBox *repeat = GTK_COMBO_BOX(lookup_widget(GTK_WIDGET(button), "midOccurance"));
  const gchar *iterStr = gtk_entry_get_text(GTK_ENTRY(lookup_widget(GTK_WIDGET(button), "midSelection")));

  g_assert (treeview != NULL);
  g_assert (model != NULL);
  g_assert (type != NULL);
  g_assert (entry != NULL);
  g_assert (repeat != NULL);
  g_assert (iterStr != NULL);
  
  if (*iterStr == '\0') {
    return;
  }
  
  gtk_tree_model_get_iter_from_string (model, &iter, iterStr);
  if (gtk_combo_box_get_active(type) == REGWIZ_DONT_KNOW) {
    gtk_list_store_remove(GTK_LIST_STORE(model), &iter);
    if (gtk_tree_model_iter_n_children (model, NULL) <= 0) {
      gtk_widget_set_sensitive(lookup_widget(GTK_WIDGET(button), "modifiedSelectedContents"), FALSE);
      gtk_widget_set_sensitive(lookup_widget(GTK_WIDGET(button), "deleteSelectedContents"), FALSE);
    }
    gtk_combo_box_set_active(type, 0);
    gtk_entry_set_text(entry, "");
    gtk_combo_box_set_active(repeat, 0);
    gtk_entry_set_text(iterC, "");
    gtk_entry_set_text(GTK_ENTRY(lookup_widget(GTK_WIDGET(button), "midSelection")), "");
    return; /* Delete entry instead */
  }
    
  gtk_list_store_set (GTK_LIST_STORE(model), &iter,
                      REGEX_TYPE_COLUMN, gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT(type)),
                      REGEX_ENTRY_COLUMN, gtk_entry_get_text(entry),
                      REGEX_REPEAT_COLUMN, gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT(repeat)),
                      REGEX_TYPE_INT_COLUMN, gtk_combo_box_get_active(type),
                      REGEX_REPEAT_INT_COLUMN, gtk_combo_box_get_active(repeat),
                      -1);
    gtk_combo_box_set_active(type, 0);
    gtk_entry_set_text(entry, "");
    gtk_combo_box_set_active(repeat, 0);
    gtk_entry_set_text(iterC, "");
    gtk_entry_set_text(GTK_ENTRY(lookup_widget(GTK_WIDGET(button), "midSelection")), "");
}


void
on_midSelection_changed                (GtkEditable     *editable,
                                        gpointer         user_data)
{
  const gchar *selection = gtk_entry_get_text(GTK_ENTRY(editable));
  
  if (*selection != '\0') {
    gtk_widget_set_sensitive(lookup_widget(GTK_WIDGET(editable), "updateSelectedContents"), TRUE);
  } else {
    gtk_widget_set_sensitive(lookup_widget(GTK_WIDGET(editable), "updateSelectedContents"), FALSE);
  }
}


void
on_expWizard_realize                   (GtkWidget       *widget,
                                        gpointer         user_data)
{
  gint widgetType = (intptr_t)g_object_get_data(G_OBJECT(widget), "regexType");

  if (widgetType == FILE_REGEX_TYPE) {
    gtk_window_set_title (GTK_WINDOW (fileRegexWizard), _("File Expression Wizard"));
  } else if (widgetType == CONTEXT_REGEX_TYPE) {
    gtk_window_set_title (GTK_WINDOW (contextRegexWizard), _("Context Expression Wizard"));
  }
  
  gtk_combo_box_set_active(GTK_COMBO_BOX(lookup_widget(widget, "startType")), REGWIZ_DONT_KNOW);
  gtk_combo_box_set_active(GTK_COMBO_BOX(lookup_widget(widget, "startOccurance")), REGWIZ_REPEAT_ONCE);
  gtk_combo_box_set_active(GTK_COMBO_BOX(lookup_widget(widget, "midType")), REGWIZ_DONT_KNOW);
  gtk_combo_box_set_active(GTK_COMBO_BOX(lookup_widget(widget, "midOccurance")), REGWIZ_REPEAT_ONCE);
  gtk_combo_box_set_active(GTK_COMBO_BOX(lookup_widget(widget, "endType")), REGWIZ_DONT_KNOW);
  gtk_combo_box_set_active(GTK_COMBO_BOX(lookup_widget(widget, "endOccurance")), REGWIZ_REPEAT_ONCE);
}


void
on_midTreeView_drag_end                (GtkWidget       *widget,
                                        GdkDragContext  *drag_context,
                                        gpointer         user_data)
{
    updateRegExWizard(widget);
}


void
on_configDialog_realize                (GtkWidget       *widget,
                                        gpointer         user_data)
{
    realize_configDialog(widget);
}


void
on_autoFindExe_clicked                 (GtkButton       *button,
                                        gpointer         user_data)
{
  GtkWidget *dialog = create_autoComplete();
  GtkProgressBar *pbar = GTK_PROGRESS_BAR(lookup_widget(dialog, "progressbar3"));
  userExeData *exeData = g_malloc0(sizeof(userExeData));

  /* Attach data to the dialog */
  exeData->parent = GTK_WIDGET(button);
  gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog), GTK_RESPONSE_OK, FALSE);
  gtk_progress_bar_set_fraction(pbar, 0);
  gtk_progress_bar_set_text(pbar, _("0%"));
  g_object_set_data_full(G_OBJECT(dialog), "exeData", exeData, g_free_exeData);

  /* Spawn idle iterator, and show status dialog */
  exeData->gid = g_timeout_add(40, getExeData, dialog); /* Slow iterations down by using timeouts */
  gtk_widget_show(dialog);
}


void
on_online_release_notes1_activate      (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  /* Browse to a new release notes subset. Note redirect required by website */
  gchar *website = g_strdup_printf(_("https://sourceforge.net/projects/searchmonkey/files/gSearchmonkey%20GTK%20%28Gnome%29/0.8.2%20%5Bstable%5D/"), PACKAGE, VERSION);
  SMsyscall(website, BROWSER_LIST);
}


void
on_configResetAll_clicked              (GtkButton       *button,
                                        gpointer         user_data)
{
  gchar *searchmonkeyExe = g_object_get_data(G_OBJECT(mainWindowApp), "argvPointer");
  GtkWidget *confirmDialog = gtk_message_dialog_new (GTK_WINDOW(lookup_widget(GTK_WIDGET(button), "configDialog")),
                                                     (GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
                                                     GTK_MESSAGE_WARNING,
                                                     GTK_BUTTONS_OK_CANCEL,
                                                     _("Are you sure that you wish to delete the config file, and restart searchmonkey?"));

  if (gtk_dialog_run (GTK_DIALOG (confirmDialog)) == GTK_RESPONSE_OK) {

    /* Delete the config file, and disable saving on exit - attempt to restart searchmonkey too? */
    if (g_remove(gConfigFile) != 0) {
      g_print(_("Error! Unable to remove config file %s.\n"), gConfigFile);    
    }
    g_object_set_data(G_OBJECT(mainWindowApp), CONFIG_DISABLE_SAVE_CONFIG_STRING, GINT_TO_POINTER(CONFIG_DISABLE_SAVE_CONFIG));
    g_spawn_command_line_async (searchmonkeyExe, NULL); /* Assume it worked */
    gtk_dialog_response (GTK_DIALOG(lookup_widget(GTK_WIDGET(button), "configDialog")), GTK_RESPONSE_REJECT);
    gtk_main_quit(); /* Exit this instance */
    return;
  }
  gtk_widget_destroy (confirmDialog);  
}


void
on_configSaveNow_clicked               (GtkButton       *button,
                                        gpointer         user_data)
{
  GKeyFile *keyString = getGKeyFile(GTK_WIDGET(mainWindowApp));

  /* Immediately unrealize check button */
  unrealizeToggle(GTK_WIDGET(button), keyString, "configuration", "configPromptSave");
  
  storeGKeyFile(keyString);
  setConfigFileLocation (GTK_WIDGET(button));
}


void
on_forums1_activate                    (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  SMsyscall(_("http://searchmonkey.embeddediq.com/index.php/support/index"), BROWSER_LIST);
}


void
on_matches1_activate                   (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(menuitem))) {
    columnClick(GTK_WIDGET(menuitem), MATCHES_COUNT_COLUMN);
  }
}


void
on_window1_unrealize                   (GtkWidget       *widget,
                                        gpointer         user_data)
{
  unrealize_searchmonkeyWindow(widget); /* Initialise everything possible immediately */
}


void
on_window1_realize                     (GtkWidget       *widget,
                                        gpointer         user_data)
{
  realize_searchmonkeyWindow(widget); /* Initialise everything possible immediately */
}

gboolean
on_configResultEOL_focus_out_event     (GtkWidget       *widget,
                                        GdkEventFocus   *event,
                                        gpointer         user_data)
{
  checkCsvEntry(GTK_ENTRY(lookup_widget(widget, "configResultEOL")));
  return FALSE;
}



gboolean
on_configResultEOF_focus_out_event     (GtkWidget       *widget,
                                        GdkEventFocus   *event,
                                        gpointer         user_data)
{
  checkCsvEntry(GTK_ENTRY(lookup_widget(widget, "configResultEOF")));
  return FALSE;
}


gboolean
on_configResultDelimiter_focus_out_event
                                        (GtkWidget       *widget,
                                        GdkEventFocus   *event,
                                        gpointer         user_data)
{
  checkCsvEntry(GTK_ENTRY(lookup_widget(widget, "configResultDelimiter")));
  return FALSE;
}


void
on_newInstance1_activate               (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  spawnNewSearchmonkey();
}


void
on_playButton_activate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  start_search_thread(GTK_WIDGET(menuitem));
}


void
on_stopButton_activate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  stop_search_thread(GTK_WIDGET(menuitem));
}


void
on_horizontal_results1_activate        (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(menuitem))) {
    setResultsViewHorizontal(GTK_WIDGET(menuitem), TRUE);
  }
}


void
on_vertical_results1_activate          (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{ 
  if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(menuitem))) {
    setResultsViewHorizontal(GTK_WIDGET(menuitem), FALSE); /* default to vertical */
  }
}



void
on_newInstance2_clicked                (GtkToolButton   *toolbutton,
                                        gpointer         user_data)
{
  spawnNewSearchmonkey();
}



void
on_playButton_clicked                  (GtkButton       *button,
                                        gpointer         user_data)
{
  start_search_thread(GTK_WIDGET(button));
}


void
on_stopButton_clicked                  (GtkButton       *button,
                                        gpointer         user_data)
{
  stop_search_thread(GTK_WIDGET(button));
}



void
on_autosize_columns_activate           (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  GKeyFile *keyString = getGKeyFile(GTK_WIDGET(mainWindowApp));
  gboolean autoColumnWidth = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(menuitem));

  g_assert(keyString != NULL);
  
  realizeTreeviewColumns (GTK_WIDGET(mainWindowApp), keyString, "history", "treeview", autoColumnWidth);  
}

/* surprise : menu empty at last 2017
 so I've made the job - :-) Luc A. - 4 janv 2018
*/


void
on_edit_file1_activate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
 GtkTreeView *treeView;
 gchar *fullFileName;
  
  if (getResultsViewHorizontal(GTK_WIDGET(menuitem))) {
    treeView = GTK_TREE_VIEW(lookup_widget(GTK_WIDGET(menuitem), "treeview1"));
  } else {
    treeView = GTK_TREE_VIEW(lookup_widget(GTK_WIDGET(menuitem), "treeview2"));
  }
  fullFileName = getFullFileName(treeView, FULL_FILENAME_COLUMN);

  if (fullFileName != NULL) {
    SMsyscall(fullFileName, TEXTEDITOR_LIST);
    g_free(fullFileName);
  }

}

/* surprise : menu empty at last 2017
 so I've made the job - :-) Luc A. - 4 janv 2018
*/


void
on_open_folder1_activate               (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
 GtkTreeView *treeView;
  gchar *location;
  
  if (getResultsViewHorizontal(GTK_WIDGET(menuitem))) {
    treeView = GTK_TREE_VIEW(lookup_widget(GTK_WIDGET(menuitem), "treeview1"));
  } else {
    treeView = GTK_TREE_VIEW(lookup_widget(GTK_WIDGET(menuitem), "treeview2"));
  }
  location = getFullFileName(treeView, LOCATION_COLUMN);

  if (location != NULL) {
    SMsyscall(location, FILEEXPLORER_LIST);
    g_free(location);
  }
}


void
on_folderDepthCheck_toggled            (GtkToggleButton *togglebutton,
                                        gpointer         user_data)
{
  gboolean checked = gtk_toggle_button_get_active(togglebutton);

  gtk_widget_set_sensitive(lookup_widget(GTK_WIDGET(togglebutton), "folderDepthSpin"), checked); /* Control spin sensitivity */
  if (checked) { /* Re-enable folder recursion enable, if depth set */
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(lookup_widget(GTK_WIDGET(togglebutton), "searchSubfoldersCheck")), TRUE);
  }
}


void
on_searchSubfoldersCheck_toggled       (GtkToggleButton *togglebutton,
                                        gpointer         user_data)
{
  if (!gtk_toggle_button_get_active(togglebutton)) { /* Disable depth limit, if recurse disabled */
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(lookup_widget(GTK_WIDGET(togglebutton), "folderDepthCheck")), FALSE);
  }
}


void
on_dosExpressionRadioFile_clicked      (GtkButton       *button,
                                        gpointer         user_data)
{
  GtkStatusbar *statusbar = GTK_STATUSBAR(lookup_widget(GTK_WIDGET(button), "statusbar1"));
  gchar *tmpstr;
  const gint tmpLimit = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(lookup_widget(GTK_WIDGET(button), "maxHitsSpinResults")));
  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(lookup_widget(GTK_WIDGET(button), "limitResultsCheckResults")))) {
       tmpstr= g_strdup_printf(_("/Max hits limit:%d"), tmpLimit);
  }
  else {
       tmpstr=  g_strdup_printf(_("/No Max hits limit"));
  }

  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button))) {
    changeModel(GTK_WIDGET(button), "regex", "noregex");
  }
  gtk_statusbar_pop(statusbar, STATUSBAR_CONTEXT_ID(statusbar));  
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (lookup_widget(GTK_WIDGET(button), "dosExpressionRadioFile"))))
    {        
        gtk_statusbar_push(statusbar, STATUSBAR_CONTEXT_ID(statusbar),
                     g_strdup_printf(_("%s/research mode with jokers(DOS like)"), tmpstr));
    }
  else  gtk_statusbar_push(statusbar, STATUSBAR_CONTEXT_ID(statusbar),
                     g_strdup_printf(_("%s/research mode with RegEx"), tmpstr)); 
  g_free(tmpstr);
}


void
on_regularExpressionRadioFile_clicked  (GtkButton       *button,
                                        gpointer         user_data)
{
  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button))) {
    changeModel(GTK_WIDGET(button), "noregex", "regex");
  }
}

void
on_IntervalStartBtn_clicked            (GtkButton       *button,
                                        gpointer         user_data)
{
  GDate date_interval1, date_interval2;
  gboolean fErrDateInterval1 = FALSE;
  gboolean fErrDateInterval2 = FALSE;
  GtkWidget *dateEntry = lookup_widget(GTK_WIDGET(button), "intervalStartEntry");
  gchar* newDate = getDate(gtk_button_get_label(dateEntry), lookup_widget(GTK_WIDGET(button), "window1"));
  gtk_button_set_label(dateEntry, newDate);

 /* we must check if dates aren't inverted */

  gchar *endDate=NULL;
  endDate = gtk_button_get_label(lookup_widget(GTK_WIDGET(button), "intervalEndEntry"));
  g_date_set_parse(&date_interval1, newDate);
  g_date_set_parse(&date_interval2, endDate);
  fErrDateInterval1 = g_date_valid(&date_interval1);
  if(!fErrDateInterval1)
      printf("Internal erreor date Interv 1\n");
  fErrDateInterval2 = g_date_valid(&date_interval2);  
  if(!fErrDateInterval2)
      printf("Internal error date Interv 2\n");
  gint cmp_date = g_date_compare (&date_interval1,&date_interval2);
  if(cmp_date>0)
    {
       miscErrorDialog(lookup_widget(GTK_WIDGET(button), "window1"), _("<b>Error!</b>\n\nDates mismatch ! 'Before than' date must be <i>more recent</i> than 'After than' date.\n\nPlease check the dates."));
       gtk_button_set_label(lookup_widget(GTK_WIDGET(button), "intervalStartEntry"), endDate);
    }

  g_free(newDate);
  //g_free(endDate);
}

void
on_IntervalEndBtn_clicked            (GtkButton       *button,
                                        gpointer         user_data)
{
  GDate date_interval1, date_interval2;
  gboolean fErrDateInterval1 = FALSE;
  gboolean fErrDateInterval2 = FALSE;
  GtkWidget *dateEntry = lookup_widget(GTK_WIDGET(button), "intervalEndEntry");
  gchar* newDate = getDate(gtk_button_get_label(dateEntry), lookup_widget(GTK_WIDGET(button), "window1"));
  gtk_button_set_label(dateEntry, newDate);

 /* we must check if dates aren't inverted */
  gchar *startDate=NULL;
  startDate = gtk_button_get_label(lookup_widget(GTK_WIDGET(button), "intervalStartEntry"));
  g_date_set_parse(&date_interval1, startDate);
  g_date_set_parse(&date_interval2, newDate);
  fErrDateInterval1 = g_date_valid(&date_interval1);
  if(!fErrDateInterval1)
      printf("Internal error date Interv 1\n");
  fErrDateInterval2 = g_date_valid(&date_interval2);  
  if(!fErrDateInterval2)
      printf("Internal error date Interv 2\n");
  gint cmp_date = g_date_compare (&date_interval1,&date_interval2);
  if(cmp_date>0)
    {
       miscErrorDialog(lookup_widget(GTK_WIDGET(button), "window1"), _("<b>Error!</b>\n\nDates mismatch ! 'Before than' date must be <i>more recent</i> than 'After than' date.\n\nPlease check the dates."));
       gtk_button_set_label(lookup_widget(GTK_WIDGET(button), "intervalEndEntry"), startDate);
    }

  g_free(newDate);
}

void
on_afterCalenderBtn_clicked            (GtkButton       *button,
                                        gpointer         user_data)
{
  GtkWidget *dateEntry = lookup_widget(GTK_WIDGET(button), "afterEntry");
  gchar* newDate = getDate(gtk_button_get_label(dateEntry), lookup_widget(GTK_WIDGET(button), "window1"));
  gtk_button_set_label(dateEntry, newDate);
  g_free(newDate);
}


void
on_beforeCalendatBtn_clicked           (GtkButton       *button,
                                        gpointer         user_data)
{
  GtkWidget *dateEntry = lookup_widget(GTK_WIDGET(button), "beforeEntry");
  gchar* newDate = getDate(gtk_button_get_label(dateEntry),lookup_widget(GTK_WIDGET(button), "window1"));
  gtk_button_set_label(dateEntry, newDate);
  g_free(newDate);
}

/* Luc A - janv 2018 */
gboolean
on_LessThanSize_focus_out_event         (GtkWidget       *widget,
                                        GdkEventFocus   *event,
                                        gpointer         user_data)
{
  gchar * regexp;
  guint flags;
  // printf("combo Less changée \n");  
  return FALSE;
}

/* Luc A - janv 2018 */
gboolean
on_MoreThanSize_focus_out_event         (GtkWidget       *widget,
                                        GdkEventFocus   *event,
                                        gpointer         user_data)
{
  gchar * regexp;
  guint flags;
  // printf("combo More changée \n");
  return FALSE;
}

gboolean on_folder_focus_out_event(GtkWidget       *widget,
                                        GdkEventFocus   *event,
                                        gpointer         user_data)
{
 gchar *filename;

 filename = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT(widget));
 /* we must change the tooltip */
 gtk_widget_set_tooltip_text (GTK_WIDGET(lookup_widget(GTK_WIDGET(widget), "lookIn")),filename ); 
 return FALSE;
}

gboolean  on_folder_query_tooltip_event(GtkWidget  *widget,
               gint        x,
               gint        y,
               gboolean    keyboard_mode,
               GtkTooltip *tooltip,
               gpointer    user_data)
{
 gchar *filename;

 filename = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT(widget));
 /* we must change the tooltip */
 gtk_tooltip_set_markup (GTK_TOOLTIP(tooltip),g_strconcat(_("<b><u>Look in :</u></b>\n") ,filename, NULL) ); 
 gtk_tooltip_set_icon_from_icon_name (GTK_TOOLTIP(tooltip),
                                     "gtk-open",
                                     GTK_ICON_SIZE_DIALOG);
 
 return TRUE;
}

gboolean
on_regexp_focus_out_event              (GtkWidget       *widget,
                                        GdkEventFocus   *event,
                                        gpointer         user_data)
{
  gchar * regexp;
  guint flags;
  gchar* error = _("Error! Invalid 'file name' regular expression");
  
  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(lookup_widget(widget, "regularExpressionRadioFile")))) {
    if (getExtendedRegexMode(widget)) {
      flags |= REG_EXTENDED;
    }
    flags |= REG_NOSUB;

    regexp = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT(widget));
  
    if (test_regexp(regexp, flags, error)) {
      addUniqueRow(widget, regexp);
    }
  }
  return FALSE;
}

gboolean
on_regexp2_focus_out_event             (GtkWidget       *widget,
                                        GdkEventFocus   *event,
                                        gpointer         user_data)
{
  gchar * regexp;
  guint flags;
  gchar* error = _("Error! Invalid 'containing text' regular expression");
  
  if (getExtendedRegexMode(widget)) {
    flags |= REG_EXTENDED;
  }
  flags |= REG_NOSUB;

  regexp = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT(widget));
  
  if (test_regexp(regexp, flags, error)) {
    addUniqueRow(widget, regexp);
  }
  return FALSE;
}


void
on_limitResultsCheckResults_toggled    (GtkToggleButton *togglebutton,
                                        gpointer         user_data)
{
  gchar *tmpstr;
  gboolean active = gtk_toggle_button_get_active(togglebutton);
  GtkStatusbar *statusbar = GTK_STATUSBAR(lookup_widget(GTK_WIDGET(togglebutton), "statusbar1"));
  gtk_widget_set_sensitive(lookup_widget(GTK_WIDGET(togglebutton), "limit_results_hbox"), active);

  const gint tmpLimit = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(lookup_widget(GTK_WIDGET(togglebutton), "maxHitsSpinResults")));
  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(lookup_widget(GTK_WIDGET(togglebutton), "limitResultsCheckResults")))) {
       tmpstr= g_strdup_printf(_("/Max hits limit:%d"), tmpLimit);
  }
  else {
       tmpstr=  g_strdup_printf(_("/No Max hits limit"));
  }
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (lookup_widget(GTK_WIDGET(togglebutton), "dosExpressionRadioFile"))))
    {        
        gtk_statusbar_push(statusbar, STATUSBAR_CONTEXT_ID(statusbar),
                     g_strdup_printf(_("%s/research mode with jokers(DOS like)"), tmpstr));
    }
  else  gtk_statusbar_push(statusbar, STATUSBAR_CONTEXT_ID(statusbar),
                     g_strdup_printf(_("%s/research mode with RegEx"), tmpstr)); 
  g_free(tmpstr);
}


void
on_showLinesCheckResults_toggled       (GtkToggleButton *togglebutton,
                                        gpointer         user_data)
{
  gboolean active = gtk_toggle_button_get_active(togglebutton);
  gtk_widget_set_sensitive(lookup_widget(GTK_WIDGET(togglebutton), "show_line_contents_hbox"), active);
}



void
on_limitContentsCheckResults_toggled   (GtkToggleButton *togglebutton,
                                        gpointer         user_data)
{
  gboolean active = gtk_toggle_button_get_active(togglebutton);
  gtk_widget_set_sensitive(lookup_widget(GTK_WIDGET(togglebutton), "limit_contents_hbox"), active);	
}


void
on_autoComplete_response               (GtkDialog       *dialog,
                                        gint             response_id,
                                        gpointer         user_data)
{
  userExeData *exeData = g_object_get_data(G_OBJECT(dialog), "exeData");
  GtkWidget *parent = exeData->parent;
  GtkWidget *widget = GTK_WIDGET(dialog);
  GtkWidget *choosers[3];
  gint i,j;

  if (response_id == GTK_RESPONSE_OK) { /* Can only happen with valid data.. */
    /* Store new values into configuration */  
    choosers[BROWSER_LIST] = lookup_widget(parent, "configWebBrowser");
    choosers[TEXTEDITOR_LIST] = lookup_widget(parent, "configTextEditor");
    choosers[FILEEXPLORER_LIST] = lookup_widget(parent, "configFileExplorer");
    for (j=0; j<G_EXE_LIST_MAX_DEPTH; j++) {
      for (i=0; i<G_EXE_LIST_MAX_WIDTH; i++) {
        if (exeData->retStr[j][i] != NULL) {
          gtk_entry_set_text(GTK_ENTRY(choosers[j]), exeData->retStr[j][i]);
          break;
        }
      }
    }
  } else { /* Assume cancelled */
    g_source_remove(exeData->gid); /* Stop timeout (just in case) */
  }

  /* Clean exit */
  gtk_widget_destroy(GTK_WIDGET(dialog));
}

/*************************************************
 callback : change the preview according to 
 current value in the 'entrySince'
 spinButton
*************************************************/
gboolean on_entrySince_value_changed_event   (GtkWidget       *widget,
                                        GdkEventFocus   *event,
                                        gpointer         user_data)
{
  gchar *test = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT(lookup_widget(GTK_WIDGET(widget), "sinceUnits")));

  g_free(test);
  return FALSE;
}

/**********************************************
 a function to retrieve the current ACTIVE
  radio button 
 in the Modified date dialog
***********************************************/
void radio_button_selected (GtkWidget *widget, gpointer data) 
{
  gint i, j;
  GSList *group;
  GtkWidget *buttonAfter;
  GtkWidget *buttonBefore;
  GtkWidget *gridSince;
  GtkWidget *buttonInterval1;
  GtkWidget *buttonInterval2;
  gchar *val_spin;

    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget))) {
       group  = gtk_radio_button_get_group (GTK_RADIO_BUTTON (widget));
       i =  g_slist_index (group, widget);
    }

  /* now we deactivate options for all OTHERS lines of Widgets */
  buttonAfter = GTK_BUTTON(lookup_widget(GTK_WIDGET(widget), "afterEntry"));/* code 3*/
  buttonBefore = GTK_BUTTON(lookup_widget(GTK_WIDGET(widget), "beforeEntry"));/* code 2 */
  gridSince = lookup_widget(GTK_WIDGET(widget), "gridSince");/* code 0 */
  buttonInterval1 = GTK_BUTTON(lookup_widget(GTK_WIDGET(widget), "intervalStartEntry"));/* code 1 */
  buttonInterval2 = GTK_BUTTON(lookup_widget(GTK_WIDGET(widget), "intervalEndEntry"));
  gtk_widget_set_sensitive (buttonAfter, FALSE);
  gtk_widget_set_sensitive (buttonBefore, FALSE);
  gtk_widget_set_sensitive (gridSince, FALSE);
  gtk_widget_set_sensitive (buttonInterval1, FALSE);
  gtk_widget_set_sensitive (buttonInterval2, FALSE);

  switch(i) {
     case 0:{ gtk_widget_set_sensitive (gridSince, TRUE);
              gchar *test = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT( lookup_widget(GTK_WIDGET(widget), "sinceUnits")  ));
              val_spin = g_strdup_printf("%d", 
                             gtk_spin_button_get_value_as_int(
                                   GTK_SPIN_BUTTON(lookup_widget(GTK_WIDGET(widget), "entrySince")) ) );
              g_free(test);
              g_free(val_spin);
              break;}
     case 1:{gtk_widget_set_sensitive (buttonInterval1, TRUE);  
             gtk_widget_set_sensitive (buttonInterval2, TRUE);
             break;}
     case 2:{gtk_widget_set_sensitive (buttonBefore, TRUE);
              break;}
     case 3:{gtk_widget_set_sensitive (buttonAfter, TRUE);             
            break;}
     case 4:{break;}
     case 5:{break;}
     default:{break;}
  }/* end switch */
}


void
on_radiobuttonAll_toggled      (GtkButton       *button,
                                        gpointer         user_data)
{

  radio_button_selected(button, NULL);
}

void
on_radiobuttonToday_toggled      (GtkButton       *button,
                                        gpointer         user_data)
{

  radio_button_selected(button, NULL);

}

void
on_radiobuttonAfter_toggled      (GtkButton       *button,
                                        gpointer         user_data)
{
  radio_button_selected(button, NULL);
}

void
on_radiobuttonBefore_toggled      (GtkButton       *button,
                                        gpointer         user_data)
{
  radio_button_selected(button, NULL);
}

void
on_radiobuttonInterval_toggled      (GtkButton       *button,
                                        gpointer         user_data)
{
  radio_button_selected(button, NULL);
}

void
on_radiobuttonSince_toggled      (GtkButton       *button,
                                        gpointer         user_data)
{
  radio_button_selected(button, NULL);
}


/*******************************************
 on function to catch the file size and 
 modified date expander changing
******************************************/
void expander_callback (GObject    *object,
                   GParamSpec *param_spec,
                   gpointer    user_data)
{
  GtkExpander *expander;
  GtkLabel *label;
  GtkWidget *iconExp;
  GtkWidget *gridExp;

  expander = GTK_EXPANDER (object);
  label =  GTK_LABEL(lookup_widget(GTK_WIDGET(expander), "label_expander"));
  iconExp = lookup_widget(GTK_WIDGET(expander), "iconExpander");
  gridExp = lookup_widget(GTK_WIDGET(expander), "gridExpander");

  if (gtk_expander_get_expanded (expander))
    {
      /* we draw the title horizontally */
      gtk_label_set_angle(label, 0);
      /* we move the image */
      g_object_ref(iconExp); /* it's a kind of Push to preserve the object */
      gtk_container_remove(GTK_CONTAINER(gridExp), iconExp); /* it isn't destroyed due to the previous g_object_ref */
      gtk_grid_attach(GTK_GRID(gridExp), iconExp ,1,0,1,1);
      g_object_unref(iconExp);/* POP */
    }
  else
    {
      /* we draw the title vertically */
       gtk_label_set_angle(label, 90);
     /* we move the image */
      g_object_ref(iconExp); /* it's a kind of Push to preserve the object */
      gtk_container_remove(GTK_CONTAINER(gridExp), iconExp); /* it isn't destroyed due to the previous g_object_ref */
      gtk_grid_attach(GTK_GRID(gridExp), iconExp ,0,1,1,1);
      g_object_unref(iconExp);/* POP */
    }
}

/***********************************
 change in the advanced mode switch
***********************************/
void on_advancedMode_event(GtkSwitch *widget,
               gpointer   user_data)
{
   GtkWidget *expander = lookup_widget(GTK_WIDGET(widget), "expander_options");
   GtkWidget *btnWizar1 = lookup_widget(GTK_WIDGET(widget), "regExpWizard1");
   GtkWidget *btnWizar2 = lookup_widget(GTK_WIDGET(widget), "regExpWizard2");

   if(gtk_switch_get_active (GTK_SWITCH(widget))) {
       gtk_widget_set_sensitive(GTK_WIDGET(expander) , TRUE);
       gtk_widget_set_sensitive(GTK_WIDGET(btnWizar1) , TRUE);
       gtk_widget_set_sensitive(GTK_WIDGET(btnWizar2) , TRUE);
   }
   else {
    gtk_expander_set_expanded (GTK_EXPANDER(expander), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(expander) , FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(btnWizar1) , FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(btnWizar2) , FALSE);
   }
}
