/*
 * Phomo.cs
 * Create photomosaics from F-Spot using Phomo
 *
 * Author(s)
 * 	Peter Goetz
 *
 * This is free software. See COPYING for details
 *
 */

using System;
using System.IO;
using System.Collections;
using System.Collections.Generic;
using Gtk;

using FSpot;
using FSpot.Extensions;
using FSpot.Widgets;
using FSpot.Filters;
using FSpot.UI.Dialog;
using FSpot.Utils;
using Mono.Unix;

namespace PhomoExtension {
	
	
	public class Phomo: ICommand
	{
		class ExtensionException: Exception {
			public ExtensionException(string message) : base(message) {}
		}
		
		[Glade.Widget] Gtk.Dialog dialog;
		[Glade.Widget] Gtk.RadioButton create_new_db_radio_button;
		[Glade.Widget] Gtk.VBox create_new_database_suboptions;
		[Glade.Widget] Gtk.RadioButton tags_radio_button;
		[Glade.Widget] Gtk.HBox tags_hbox;
		FSpot.Widgets.TagEntry tag_entry;
		[Glade.Widget] Gtk.Entry aspect_ratio_x;
		[Glade.Widget] Gtk.Entry aspect_ratio_y;
		[Glade.Widget] Gtk.CheckButton save_for_later_checkbox;
		[Glade.Widget] Gtk.HBox save_for_later_hbox;
		[Glade.Widget] Gtk.Entry save_for_later_filename;
		[Glade.Widget] Gtk.FileChooserButton save_for_later_folder_chooser;
		[Glade.Widget] Gtk.FileChooserButton use_existing_file_chooser;
		[Glade.Widget] Gtk.RadioButton use_existing_db_button;
		[Glade.Widget] Gtk.SpinButton x_res_in_stones;
		[Glade.Widget] Gtk.SpinButton x_res_in_pixels;
		[Glade.Widget] Gtk.SpinButton min_distance;
		
		FSpot.UI.Dialog.ThreadProgressDialog progress_dialog;
		System.Threading.Thread command_thread;
		
		
		const string PHOMO_PATH = "phomo";
		
		public void Run (object o, EventArgs e) {
			Log.Information ("Executing Phomo extension");
			if (MainWindow.Toplevel.SelectedPhotos ().Length == 0) {
				InfoDialog (Catalog.GetString ("No selection available"),
					    Catalog.GetString ("This tool requires an active selection. Please select one or more pictures and try again"),
					    Gtk.MessageType.Error);
				return;
			}
			if (!IsBackendInstalled()) {
				InfoDialog(Catalog.GetString ("DVDSlideshow not available"),
					   Catalog.GetString ("The dvd-slideshow executable was not found in path. Please check that you have it installed and that you have permissions to execute it"),
					   Gtk.MessageType.Error);
				return;
			}
			ShowDialog ();
		}

		private bool IsBackendInstalled() {
			string output = "";
			try {
				System.Diagnostics.Process mp_check = new System.Diagnostics.Process();
				mp_check.StartInfo.RedirectStandardOutput = true;
				mp_check.StartInfo.UseShellExecute = false;
				mp_check.StartInfo.FileName = "phomo";
				mp_check.StartInfo.Arguments = "--version";
				mp_check.Start();
				mp_check.WaitForExit ();
				StreamReader sroutput = mp_check.StandardOutput;
				output = sroutput.ReadLine();
			} catch (System.Exception) {
			}
			return System.Text.RegularExpressions.Regex.IsMatch(output, "^phomo");
		}

		public void ShowDialog () {
			Glade.XML xml = new Glade.XML (null, "Phomo.glade", "dialog", "f-spot");
			if(xml == null) return;
			xml.Autoconnect (this);
			dialog.Modal = false;
			dialog.TransientFor = null;

			tag_entry = new FSpot.Widgets.TagEntry (MainWindow.Toplevel.Database.Tags, false);
			tags_hbox.Add(tag_entry);

			dialog.Response += on_dialog_response;
			create_new_db_radio_button.Toggled += on_database_toggle;
			save_for_later_checkbox.Toggled += on_save_for_later_toggle;
			tags_radio_button.Toggled += on_tags_toggle;

			
			init_controls();

			dialog.ShowAll ();
		}
		
		void init_controls() {
			try {
				using (System.IO.StreamReader file = new System.IO.StreamReader(config_file))
				{
					if(file.ReadLine() == "create_new") {
						create_new_db_radio_button.Active = true;
						if(file.ReadLine() == "tags") {
							tags_radio_button.Active = true;
							tag_entry.UpdateFromTagNames (file.ReadLine().Split('|'));
						} else {
							tags_radio_button.Active = false;
						}
						aspect_ratio_x.Text = file.ReadLine();
						aspect_ratio_y.Text = file.ReadLine();
						if(file.ReadLine() == "save_for_later_use") {
							save_for_later_checkbox.Active = true;
							save_for_later_filename.Text = file.ReadLine();
							save_for_later_folder_chooser.SetFilename(file.ReadLine());
						}
					} else {
						use_existing_db_button.Active = true;
						use_existing_file_chooser.SetFilename(file.ReadLine());
					}
					x_res_in_pixels.Value = System.Convert.ToDouble(file.ReadLine());
					x_res_in_stones.Value = System.Convert.ToDouble(file.ReadLine());
					min_distance.Value = System.Convert.ToDouble(file.ReadLine());
				}
			} catch {}

		}
		
		private void on_database_toggle(object obj, EventArgs args) {
			use_existing_file_chooser.Sensitive = !create_new_db_radio_button.Active;
			create_new_database_suboptions.Sensitive = create_new_db_radio_button.Active;
		}

		private void on_tags_toggle(object obj, EventArgs args) {
			tag_entry.Sensitive = tags_radio_button.Active;
		}

		private void on_save_for_later_toggle(object obj, EventArgs args) {
			save_for_later_folder_chooser.Sensitive = save_for_later_checkbox.Active;
			save_for_later_filename.Sensitive = save_for_later_checkbox.Active;
			save_for_later_hbox.Sensitive = save_for_later_checkbox.Active;
		}

		ArrayList tag_entry_strings = new ArrayList();
		void on_dialog_response (object obj, ResponseArgs args) {
			if (args.ResponseId == ResponseType.Ok) {
				save_controls();
				foreach (string tag_entry_string in tag_entry.GetTypedTagNames()) {
					tag_entry_strings.Add (tag_entry_string);
				}
				command_thread = new System.Threading.Thread (new System.Threading.ThreadStart (createMosaics));
				command_thread.Name = Catalog.GetString ("Creating Photomosaic");
				progress_dialog = new FSpot.UI.Dialog.ThreadProgressDialog (command_thread, 1);
				progress_dialog.Start ();
				progress_dialog.Response += HandleProgressAbort;
			}
			dialog.Destroy ();
		}

		string config_file = Path.Combine(Environment.GetFolderPath (Environment.SpecialFolder.ApplicationData), Path.Combine("f-spot", "phomo.config"));
		void save_controls() {
			try {
				using (System.IO.StreamWriter file = new System.IO.StreamWriter(config_file))
				{
					if(create_new_db_radio_button.Active) {
						file.WriteLine("create_new");
						if(tags_radio_button.Active) {
							
							file.WriteLine("tags");
							file.WriteLine(String.Join("|", tag_entry.GetTypedTagNames()));
						} else {
							file.WriteLine("current_collection");
						}
						file.WriteLine(aspect_ratio_x.Text);
						file.WriteLine(aspect_ratio_y.Text);
						if(save_for_later_checkbox.Active) {
							file.WriteLine("save_for_later_use");
							file.WriteLine(save_for_later_filename.Text);
							file.WriteLine(save_for_later_folder_chooser.Filename);
						} else {
							file.WriteLine("do_not_save_for_later_use");
						}
					} else {
						file.WriteLine("use_existing");
						file.WriteLine(use_existing_file_chooser.Filename);
					}
					file.WriteLine(x_res_in_pixels.ValueAsInt);
					file.WriteLine(x_res_in_stones.ValueAsInt);
					file.WriteLine(min_distance.ValueAsInt);
				}
			} catch {}
		}
		
		void HandleProgressAbort(object sender, Gtk.ResponseArgs args)
		{
			deleteTempFile();
			System.Console.WriteLine("Phomo Aborted");
			process.Kill();
			process.WaitForExit();
		}
	
		
		string databaseFilename = "";
		public void createMosaics () {
			int error_count = -1;
			try {
				progress_dialog.Fraction = 0.0;
				if(create_new_db_radio_button.Active) {
					if (save_for_later_checkbox.Active)	{
						databaseFilename =  System.IO.Path.Combine(save_for_later_folder_chooser.Filename, save_for_later_filename.Text);
					}
					else {
						databaseFilename = System.IO.Path.GetTempFileName();
					}
					buildDatabase(databaseFilename, aspect_ratio_x.Text + "x" + aspect_ratio_y.Text);
				} else {
					databaseFilename = use_existing_file_chooser.Filename;
				}
				progress_dialog.Fraction = 0.5;
				error_count = 0;
				double progress_slice = 0.5/MainWindow.Toplevel.SelectedPhotos ().Length;
				int i=0;
				foreach (Photo photo in MainWindow.Toplevel.SelectedPhotos ()) {
					try {
						
						string name = GetVersionName (photo);
						System.Uri outputUri = GetUriForVersionName (photo, name);
						render(databaseFilename, photo.DefaultVersionUri.LocalPath, outputUri.LocalPath, 
						       x_res_in_stones.ValueAsInt, x_res_in_pixels.ValueAsInt, min_distance.ValueAsInt, i*progress_slice, progress_slice);
						AddAndChangeToNewVersion(photo, outputUri, name);					
					} catch (Exception)	{
						error_count++;
					}
					i++;
				}		
			} catch (ExtensionException) {
				progress_dialog.ProgressText = Catalog.GetString ("Error. Could not finish.");
				progress_dialog.ButtonLabel = Gtk.Stock.Ok;
				progress_dialog.Response -= HandleProgressAbort;
			} finally {
				deleteTempFile();
				progress_dialog.Fraction = 1;
				if(error_count > -1) {
					if (error_count > 0) {
						progress_dialog.ProgressText = Catalog.GetString ("Finished with errors");
						progress_dialog.ButtonLabel = Gtk.Stock.Ok;
						progress_dialog.Response -= HandleProgressAbort;
					} else {
						Gtk.Application.Invoke (delegate { progress_dialog.Destroy(); });
					}
				}
			}
			
		}
		
		void deleteTempFile() {
			if (create_new_db_radio_button.Active && !save_for_later_checkbox.Active && databaseFilename != "") {
				try {
					System.IO.File.Delete(databaseFilename);
				} catch (System.Exception) {}
			}
		}

		void AddAndChangeToNewVersion(Photo p, System.Uri outputUri, string name) {
			p.DefaultVersionId = p.AddVersion (outputUri, name, true);
			p.Changes.DataChanged = true;
			MainWindow.Toplevel.Database.Photos.Commit (p);
		}
		System.Diagnostics.Process process;
		double step;
		double fraction;
		private void buildDatabase(string databaseFilename, string aspectRatio)
		{
			string arguments = "build-database --input-type file --photos-file - --database-filename " + 
				databaseFilename + " --aspect-ratio " + aspectRatio + " --raster-resolution 3";
			Console.WriteLine(PHOMO_PATH + " " + arguments);
			process = new System.Diagnostics.Process();
			process.StartInfo.RedirectStandardInput = true;
			process.StartInfo.RedirectStandardOutput = true;
			process.StartInfo.UseShellExecute = false;
			process.StartInfo.FileName = PHOMO_PATH;
			process.StartInfo.Arguments = arguments;
			process.OutputDataReceived += HandleOutput;
			process.Start();
			process.BeginOutputReadLine();
			StreamWriter inputStream = process.StandardInput;
			inputStream.AutoFlush = true;
			Photo [] photos = mosaicStones();
			step = 0.5/(double)(photos.Length);
			fraction = 0;
			foreach(Photo mosaicStone in photos) {
			    Console.WriteLine(mosaicStone.DefaultVersionUri.LocalPath);
				inputStream.WriteLine(mosaicStone.DefaultVersionUri.LocalPath);
			}
			Console.WriteLine();
			inputStream.WriteLine();
			inputStream.Close();
			process.WaitForExit();
			process.Close();
		}
		
		private void HandleOutput(object sendingProcess, 
            System.Diagnostics.DataReceivedEventArgs outLine)
        {
            if (!String.IsNullOrEmpty(outLine.Data))
            {
                Console.WriteLine(outLine.Data);
				if (outLine.Data.EndsWith("Added")) {
					fraction += step;
					progress_dialog.Fraction = fraction;
				}
            }
        }

		private Photo[] mosaicStones() {
			if (tags_radio_button.Active) {
				Db db = MainWindow.Toplevel.Database;
				FSpot.PhotoQuery mini_query = new FSpot.PhotoQuery (db.Photos);
				mini_query.Terms = FSpot.OrTerm.FromTags (tags(db));
				Photo [] photos = mini_query.Photos;
				if(photos.Length == 0) {
					throw new ExtensionException("No photos in current selection.");
				}
				return photos;
			} else {
				return MainWindow.Toplevel.Query.Photos;
			}
		}
		
		Tag[] tags(Db db) {
			List<Tag> taglist = new List<Tag>();
			foreach (string tag_name in tag_entry_strings) {
				Tag t = db.Tags.GetTagByName (tag_name);
				if (t != null)
					taglist.Add(t);
			}
			return taglist.ToArray();
		}
		
		private void render(string databaseFilename, string picturePath, string outputFilename, 
		                           int xResInStones, int xResInPixels, int minDistance, double progress_slice_start, double progress_slice) {
			string arguments = "render --database-filename " + GLib.Shell.Quote(databaseFilename) + " --picture-path "
				+ GLib.Shell.Quote(picturePath) + " --output-filename " + GLib.Shell.Quote(outputFilename) +" --x-resolution-in-stones " + xResInStones +
					" --output-width " + xResInPixels + " --min-distance " + minDistance;
			Console.WriteLine(PHOMO_PATH +" "+arguments);

			process = new System.Diagnostics.Process();
			process.StartInfo.RedirectStandardOutput = true;
			process.StartInfo.UseShellExecute = false;
			process.StartInfo.FileName = PHOMO_PATH;
			process.StartInfo.Arguments = arguments;
			process.Start();
			StreamReader outputStream = process.StandardOutput;
			string line = outputStream.ReadLine();
			while(line != null) {
				if (line.EndsWith("%")) {
					double progress = Convert.ToDouble(line.Replace("%", "").Replace(".", ","));
					progress_dialog.Fraction = 0.5 + progress_slice_start + progress_slice*progress/100;
				}
				line = outputStream.ReadLine();
			}

			process.WaitForExit();
			if (process.ExitCode != 0) {
				throw new Exception();
			}
			process.Close();
		}
	
		private string GetVersionName (Photo p)
		{
		    return GetVersionName (p, 1);
		}
		
		private string GetVersionName (Photo p, int i)
		{
	        string name = Catalog.GetPluralString ("PhotoMosaic", "PhotoMosaic ({0})", i);
	        name = String.Format (name, i);
	        if (p.VersionNameExists (name))
	                return GetVersionName (p, i + 1);
	        return name;
		}
		
		private System.Uri GetUriForVersionName (Photo p, string version_name)
		{
	        string name_without_ext = System.IO.Path.GetFileNameWithoutExtension (p.Name);
	        return new System.Uri (System.IO.Path.Combine (DirectoryPath (p),  name_without_ext
	                               + " (" + version_name + ")" + ".jpg"));
		}
		
        private static string DirectoryPath (Photo p)
        {
                System.Uri uri = p.VersionUri (Photo.OriginalVersionId);
                return uri.Scheme + "://" + uri.Host + System.IO.Path.GetDirectoryName (uri.AbsolutePath);
        }

		private void InfoDialog (string title, string msg, Gtk.MessageType type) {
			HigMessageDialog md = new HigMessageDialog (MainWindow.Toplevel.Window, DialogFlags.DestroyWithParent,
						  type, ButtonsType.Ok, title, msg);

			md.Run ();
			md.Destroy ();

		}

	}
}
