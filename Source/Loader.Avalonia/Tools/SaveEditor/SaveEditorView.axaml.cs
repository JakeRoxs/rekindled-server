using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Platform.Storage;
using Avalonia.Threading;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;

namespace Loader.Tools.SaveEditor
{
    public partial class SaveEditorView : UserControl
    {
        private static readonly DataFormat<SaveSlot> SlotFormat = DataFormat.CreateInProcessFormat<SaveSlot>("slot");
        private static readonly DataFormat<string> FromFormat = DataFormat.CreateInProcessFormat<string>("from");

        private DarkSoulsSave? _sourceSave;
        private DarkSoulsSave? _destSave;
        private SaveSlot? _selectedSlot;
        private Border? _selectedSlotBorder;
        private bool _copyRight = true;
        private readonly List<BankEntry> _bank = new();

        private class BankEntry
        {
            public SaveSlot Slot { get; }
            public Border Card { get; }
            public int SlotIndex { get; }
            public BankEntry(SaveSlot slot, Border card, int slotIndex)
            {
                Slot = slot;
                Card = card;
                SlotIndex = slotIndex;
            }
        }

        public SaveEditorView()
        {
            InitializeComponent();
            DragDrop.AddDragOverHandler(SourceDropPanel, SourcePanel_DragOver);
            DragDrop.AddDropHandler(SourceDropPanel, SourcePanel_Drop);
            DragDrop.AddDragOverHandler(DestDropPanel, DestPanel_DragOver);
            DragDrop.AddDropHandler(DestDropPanel, DestPanel_Drop);
        }

        private async void SourceLoadButton_OnClick(object? sender, RoutedEventArgs e)
        {
            var file = await PickSaveFileAsync();
            if (file != null)
            {
                try
                {
                    _sourceSave = new DarkSoulsSave(file.Path.LocalPath);
                    SourcePathText.Text = file.Name;
                    SourceSteamIdText.Text = _sourceSave.SteamID.ToString();
                    SourceSlotCountText.Text = $"({GetOccupiedSlotCount(_sourceSave)}/10)";
                    SourceEmptyState.IsVisible = false;
                    SourceErrorBanner.IsVisible = false;
                    RefreshSlots(SourceSlotsPanel, _sourceSave, isSource: true);
                }
                catch (System.Exception ex)
                {
                    SourceErrorBanner.IsVisible = true;
                    SourceErrorText.Text = $"Failed to load {file.Name}: {ex.Message}";
                    SourcePathText.Text = file.Name;
                }
            }
        }

        private async void DestLoadButton_OnClick(object? sender, RoutedEventArgs e)
        {
            var file = await PickSaveFileAsync();
            if (file != null)
            {
                try
                {
                    _destSave = new DarkSoulsSave(file.Path.LocalPath);
                    DestPathText.Text = file.Name;
                    DestSteamIdText.Text = _destSave.SteamID.ToString();
                    DestSlotCountText.Text = $"({GetOccupiedSlotCount(_destSave)}/10)";
                    DestEmptyState.IsVisible = false;
                    DestErrorBanner.IsVisible = false;
                    RefreshSlots(DestSlotsPanel, _destSave, isSource: false);
                }
                catch (System.Exception ex)
                {
                    DestErrorBanner.IsVisible = true;
                    DestErrorText.Text = $"Failed to load {file.Name}: {ex.Message}";
                    DestPathText.Text = file.Name;
                }
            }
        }

        private void SourceSaveButton_OnClick(object? sender, RoutedEventArgs e)
        {
            if (_sourceSave == null) return;
            try
            {
                DarkSoulsSave.Backup(_sourceSave.Path);
                _sourceSave.Write();
                ShowSaveSuccess(SourceSaveSuccessText);
            }
            catch (System.Exception ex)
            {
                SourceErrorBanner.IsVisible = true;
                SourceErrorText.Text = $"Save error: {ex.Message}";
            }
        }

        private void DestSaveButton_OnClick(object? sender, RoutedEventArgs e)
        {
            if (_destSave == null) return;
            try
            {
                DarkSoulsSave.Backup(_destSave.Path);
                _destSave.Write();
                ShowSaveSuccess(DestSaveSuccessText);
            }
            catch (System.Exception ex)
            {
                DestErrorBanner.IsVisible = true;
                DestErrorText.Text = $"Save error: {ex.Message}";
            }
        }

        private void SwapDirectionButton_OnClick(object? sender, RoutedEventArgs e)
        {
            _copyRight = !_copyRight;
            CopyRightButton.Content = _copyRight ? "→" : "←";
            SourceLabel.Text = _copyRight ? "Source Save" : "Destination Save";
            DestLabel.Text = _copyRight ? "Destination Save" : "Source Save";
        }

        private async Task<IStorageFile?> PickSaveFileAsync()
        {
            var top = TopLevel.GetTopLevel(this);
            if (top == null) return null;
            var options = new FilePickerOpenOptions
            {
                Title = "Select Save File (DS2/DS3)",
                FileTypeFilter = new List<FilePickerFileType>
                {
                    new FilePickerFileType("Save File") { Patterns = new List<string> { "*.sl2", "*.rds" } },
                    new FilePickerFileType("All Files") { Patterns = new List<string> { "*.*" } }
                }
            };

            var files = await top.StorageProvider.OpenFilePickerAsync(options);
            return files.FirstOrDefault();
        }

        private int GetOccupiedSlotCount(DarkSoulsSave save)
        {
            return save.Menu.OccupiedSlots.Count(x => x);
        }

        private void RefreshSlots(StackPanel panel, DarkSoulsSave save, bool isSource)
        {
            panel.Children.Clear();
            for (int i = 0; i < 10; i++)
            {
                if (!save.Menu.OccupiedSlots[i]) continue;
                var slot = save.Slots[i];
                var card = CreateSlotCard(slot, i, isSource);
                panel.Children.Add(card);
            }

            if (isSource)
                SourceSlotCountText.Text = $"({GetOccupiedSlotCount(save)}/10)";
            else
                DestSlotCountText.Text = $"({GetOccupiedSlotCount(save)}/10)";

            MarkDuplicates();
        }

        private void MarkDuplicates()
        {
            if (_sourceSave == null || _destSave == null) return;

            var destNames = new HashSet<(string Name, int Level)>();
            for (int i = 0; i < 10; i++)
            {
                if (_destSave.Menu.OccupiedSlots[i] && _destSave.Slots[i] != null)
                {
                    destNames.Add((_destSave.Slots[i].CharName, _destSave.Slots[i].SoulLevel));
                }
            }

            var sourceNames = new HashSet<(string Name, int Level)>();
            for (int i = 0; i < 10; i++)
            {
                if (_sourceSave.Menu.OccupiedSlots[i] && _sourceSave.Slots[i] != null)
                {
                    sourceNames.Add((_sourceSave.Slots[i].CharName, _sourceSave.Slots[i].SoulLevel));
                }
            }

            foreach (var child in SourceSlotsPanel.Children)
            {
                if (child is Border card && card.Tag is SaveSlot slot)
                {
                    var key = (slot.CharName, slot.SoulLevel);
                    if (destNames.Contains(key))
                    {
                        card.Classes.Add("duplicate");
                    }
                }
            }

            foreach (var child in DestSlotsPanel.Children)
            {
                if (child is Border card && card.Tag is SaveSlot slot)
                {
                    var key = (slot.CharName, slot.SoulLevel);
                    if (sourceNames.Contains(key))
                    {
                        card.Classes.Add("duplicate");
                    }
                }
            }
        }

        private Border CreateSlotCard(SaveSlot slot, int index, bool isSource)
        {
            var card = new Border { Classes = { "slot-card" } };
            card.PointerPressed += (s, e) =>
            {
                var data = new DataTransfer();
                data.Add(DataTransferItem.Create(SlotFormat, slot));
                data.Add(DataTransferItem.Create(FromFormat, isSource ? "source" : "dest"));
                DragDrop.DoDragDropAsync(e, data, DragDropEffects.Copy);
            };

            var grid = new Grid();
            grid.ColumnDefinitions = new ColumnDefinitions
            {
                new ColumnDefinition(GridLength.Star),
                new ColumnDefinition(GridLength.Auto)
            };

            var info = new StackPanel { Spacing = 4 };
            info.Children.Add(new TextBlock { Text = slot.CharName, Classes = { "slot-name" } });
            info.Children.Add(new TextBlock { Text = $"Level {slot.SoulLevel} | Slot {index}", Classes = { "slot-info" } });

            var timeSpan = System.TimeSpan.FromSeconds(slot.PlaytimeSeconds);
            info.Children.Add(new TextBlock
            {
                Text = $"Playtime: {timeSpan.Days}d {timeSpan.Hours}h {timeSpan.Minutes}m",
                Classes = { "slot-info" }
            });

            Grid.SetColumn(info, 0);
            grid.Children.Add(info);

            var actions = new StackPanel { Orientation = Avalonia.Layout.Orientation.Horizontal, Spacing = 8 };
            var selectBtn = new Button { Content = "Select", Classes = { "slot-action" } };
            selectBtn.Click += (s, e) => SelectSlot(slot, card, isSource);
            actions.Children.Add(selectBtn);

            var bankBtn = new Button { Content = "→ Bank", Classes = { "slot-action" } };
            bankBtn.Click += (s, e) => AddToBank(slot, index);
            actions.Children.Add(bankBtn);

            var deleteBtn = new Button { Content = "Delete", Classes = { "slot-action" } };
            deleteBtn.Click += (s, e) => DeleteSlot(index, isSource);
            actions.Children.Add(deleteBtn);

            Grid.SetColumn(actions, 1);
            grid.Children.Add(actions);

            card.Child = grid;
            card.Tag = slot;
            return card;
        }

        private void DeleteSlot(int index, bool isSource)
        {
            var save = isSource ? _sourceSave : _destSave;
            if (save == null) return;

            save.Slots[index] = null;
            save.Menu.OccupiedSlots[index] = false;

            if (isSource)
                RefreshSlots(SourceSlotsPanel, save, true);
            else
                RefreshSlots(DestSlotsPanel, save, false);
        }

        private void SelectSlot(SaveSlot slot, Border card, bool isSource)
        {
            if (_selectedSlotBorder != null)
            {
                _selectedSlotBorder.Classes.Remove("selected");
            }
            _selectedSlotBorder = card;
            card.Classes.Add("selected");
            _selectedSlot = slot;
        }

        private void AddToBank(SaveSlot slot, int slotIndex)
        {
            var card = CreateBankCard(slot, slotIndex);
            BankPanel.Children.Add(card);
            _bank.Add(new BankEntry(slot, card, slotIndex));
        }

        private Border CreateBankCard(SaveSlot slot, int slotIndex)
        {
            var card = new Border { Classes = { "slot-card" } };
            card.PointerPressed += (s, e) =>
            {
                var data = new DataTransfer();
                data.Add(DataTransferItem.Create(SlotFormat, slot));
                data.Add(DataTransferItem.Create(FromFormat, "bank"));
                DragDrop.DoDragDropAsync(e, data, DragDropEffects.Copy);
            };

            var grid = new Grid();
            grid.ColumnDefinitions = new ColumnDefinitions
            {
                new ColumnDefinition(GridLength.Star),
                new ColumnDefinition(GridLength.Auto)
            };

            var info = new StackPanel { Spacing = 2 };
            info.Children.Add(new TextBlock { Text = slot.CharName, Classes = { "slot-name" } });
            info.Children.Add(new TextBlock { Text = $"Level {slot.SoulLevel}", Classes = { "slot-info" } });
            info.Children.Add(new TextBlock { Text = $"From Slot {slotIndex}", Classes = { "slot-info" } });

            Grid.SetColumn(info, 0);
            grid.Children.Add(info);

            var actions = new StackPanel { Orientation = Avalonia.Layout.Orientation.Horizontal, Spacing = 2 };

            var toSrcBtn = new Button { Content = "L", Classes = { "bank-btn" } };
            toSrcBtn.Click += (s, e) => LoadBankSlotToSave(slot, _sourceSave, SourceSlotsPanel, true);
            actions.Children.Add(toSrcBtn);

            var toDestBtn = new Button { Content = "R", Classes = { "bank-btn" } };
            toDestBtn.Click += (s, e) => LoadBankSlotToSave(slot, _destSave, DestSlotsPanel, false);
            actions.Children.Add(toDestBtn);

            var deleteBtn = new Button { Content = "✕", Classes = { "bank-btn" } };
            deleteBtn.Click += (s, e) => RemoveFromBank(card, slot);
            actions.Children.Add(deleteBtn);

            Grid.SetColumn(actions, 1);
            grid.Children.Add(actions);

            card.Child = grid;
            return card;
        }

        private void RemoveFromBank(Border card, SaveSlot slot)
        {
            BankPanel.Children.Remove(card);
            _bank.RemoveAll(entry => entry.Card == card);
        }

        private void LoadBankSlotToSave(SaveSlot slot, DarkSoulsSave? save, StackPanel panel, bool isSource)
        {
            if (save == null)
            {
                if (isSource)
                    SourcePathText.Text = "Load a save first";
                else
                    DestPathText.Text = "Load a save first";
                return;
            }

            var emptySlot = -1;
            for (int i = 0; i < 10; i++)
            {
                if (!save.Menu.OccupiedSlots[i])
                {
                    emptySlot = i;
                    break;
                }
            }

            if (emptySlot == -1)
            {
                if (isSource)
                    SourcePathText.Text = "No empty slots";
                else
                    DestPathText.Text = "No empty slots";
                return;
            }

            save.Slots[emptySlot] = slot;
            save.Menu.OccupiedSlots[emptySlot] = true;
            save.Menu.SlotData[emptySlot] = slot.MenuData;
            RefreshSlots(panel, save, isSource);
            if (isSource)
                SourcePathText.Text = $"Loaded '{slot.CharName}' to slot {emptySlot}";
            else
                DestPathText.Text = $"Loaded '{slot.CharName}' to slot {emptySlot}";
        }

        private void ClearBankButton_OnClick(object? sender, RoutedEventArgs e)
        {
            BankPanel.Children.Clear();
            _bank.Clear();
        }

        private void CopyRightButton_OnClick(object? sender, RoutedEventArgs e)
        {
            if (_selectedSlot == null) return;

            DarkSoulsSave? source, dest;
            StackPanel sourcePanel, destPanel;
            TextBlock destTextBlock;
            TextBlock slotCountText;

            if (_copyRight)
            {
                source = _sourceSave;
                dest = _destSave;
                sourcePanel = SourceSlotsPanel;
                destPanel = DestSlotsPanel;
                destTextBlock = DestPathText;
                slotCountText = DestSlotCountText;
            }
            else
            {
                source = _destSave;
                dest = _sourceSave;
                sourcePanel = DestSlotsPanel;
                destPanel = SourceSlotsPanel;
                destTextBlock = SourcePathText;
                slotCountText = SourceSlotCountText;
            }

            if (source == null || dest == null)
            {
                destTextBlock.Text = "Load both saves first";
                return;
            }

            var emptySlot = -1;
            for (int i = 0; i < 10; i++)
            {
                if (!dest.Menu.OccupiedSlots[i])
                {
                    emptySlot = i;
                    break;
                }
            }

            if (emptySlot == -1)
            {
                destTextBlock.Text = "No empty slots in destination save";
                return;
            }

            dest.Slots[emptySlot] = _selectedSlot;
            dest.Menu.OccupiedSlots[emptySlot] = true;
            dest.Menu.SlotData[emptySlot] = _selectedSlot.MenuData;
            RefreshSlots(destPanel, dest, !_copyRight);
            slotCountText.Text = $"({GetOccupiedSlotCount(dest)}/10)";
            destTextBlock.Text = $"Copied '{_selectedSlot.CharName}' to slot {emptySlot}";
        }

        private void MoveAllButton_OnClick(object? sender, RoutedEventArgs e)
        {
            DarkSoulsSave? source, dest;
            StackPanel sourcePanel, destPanel;
            TextBlock destTextBlock;
            TextBlock slotCountText;

            if (_copyRight)
            {
                source = _sourceSave;
                dest = _destSave;
                sourcePanel = SourceSlotsPanel;
                destPanel = DestSlotsPanel;
                destTextBlock = DestPathText;
                slotCountText = DestSlotCountText;
            }
            else
            {
                source = _destSave;
                dest = _sourceSave;
                sourcePanel = DestSlotsPanel;
                destPanel = SourceSlotsPanel;
                destTextBlock = SourcePathText;
                slotCountText = SourceSlotCountText;
            }

            if (source == null || dest == null)
            {
                destTextBlock.Text = "Load both saves first";
                return;
            }

            int copied = 0;
            for (int i = 0; i < 10; i++)
            {
                if (!source.Menu.OccupiedSlots[i]) continue;

                int emptySlot = -1;
                for (int j = 0; j < 10; j++)
                {
                    if (!dest.Menu.OccupiedSlots[j])
                    {
                        emptySlot = j;
                        break;
                    }
                }

                if (emptySlot == -1) break;

                var slot = source.Slots[i];
                dest.Slots[emptySlot] = slot;
                dest.Menu.OccupiedSlots[emptySlot] = true;
                dest.Menu.SlotData[emptySlot] = slot.MenuData;
                copied++;
            }

            RefreshSlots(destPanel, dest, !_copyRight);
            slotCountText.Text = $"({GetOccupiedSlotCount(dest)}/10)";
            destTextBlock.Text = copied > 0 ? $"Copied {copied} character(s)" : "No empty slots in destination";
        }

        private SaveSlot? GetDraggedSlot(DragEventArgs e)
        {
            foreach (var item in e.DataTransfer.Items)
            {
                foreach (var format in item.Formats)
                {
                    if (format.Equals(SlotFormat))
                    {
                        return (SaveSlot?)item.TryGetRaw(SlotFormat);
                    }
                }
            }
            return null;
        }

        private string? GetDraggedFrom(DragEventArgs e)
        {
            foreach (var item in e.DataTransfer.Items)
            {
                foreach (var format in item.Formats)
                {
                    if (format.Equals(FromFormat))
                    {
                        return (string?)item.TryGetRaw(FromFormat);
                    }
                }
            }
            return null;
        }

        private void SourcePanel_DragOver(object? sender, DragEventArgs e)
        {
            if (e.DataTransfer.Formats.Contains(SlotFormat))
                e.DragEffects = DragDropEffects.Copy;
            else
                e.DragEffects = DragDropEffects.None;
        }

        private void DestPanel_DragOver(object? sender, DragEventArgs e)
        {
            if (e.DataTransfer.Formats.Contains(SlotFormat))
                e.DragEffects = DragDropEffects.Copy;
            else
                e.DragEffects = DragDropEffects.None;
        }

        private void SourcePanel_Drop(object? sender, DragEventArgs e)
        {
            if (!e.DataTransfer.Formats.Contains(SlotFormat)) return;

            var slot = GetDraggedSlot(e);
            if (slot == null) return;

            var from = GetDraggedFrom(e);

            if (_sourceSave == null) return;
            if (from == "source") return;

            var emptySlot = -1;
            for (int i = 0; i < 10; i++)
            {
                if (!_sourceSave.Menu.OccupiedSlots[i])
                {
                    emptySlot = i;
                    break;
                }
            }

            if (emptySlot == -1)
            {
                SourcePathText.Text = "No empty slots in source save";
                return;
            }

            _sourceSave.Slots[emptySlot] = slot;
            _sourceSave.Menu.OccupiedSlots[emptySlot] = true;
            _sourceSave.Menu.SlotData[emptySlot] = slot.MenuData;
            RefreshSlots(SourceSlotsPanel, _sourceSave, true);
            SourceSlotCountText.Text = $"({GetOccupiedSlotCount(_sourceSave)}/10)";
            SourcePathText.Text = $"Dropped '{slot.CharName}' to slot {emptySlot}";

            if (from == "bank")
            {
                var entry = _bank.FirstOrDefault(en => en.Slot == slot);
                if (entry != null)
                {
                    BankPanel.Children.Remove(entry.Card);
                    _bank.Remove(entry);
                }
            }
            else if (_destSave != null)
            {
                RefreshSlots(DestSlotsPanel, _destSave, false);
            }
        }

        private void DestPanel_Drop(object? sender, DragEventArgs e)
        {
            if (!e.DataTransfer.Formats.Contains(SlotFormat)) return;

            var slot = GetDraggedSlot(e);
            if (slot == null) return;

            var from = GetDraggedFrom(e);

            if (_destSave == null) return;
            if (from == "dest") return;

            var emptySlot = -1;
            for (int i = 0; i < 10; i++)
            {
                if (!_destSave.Menu.OccupiedSlots[i])
                {
                    emptySlot = i;
                    break;
                }
            }

            if (emptySlot == -1)
            {
                DestPathText.Text = "No empty slots in dest save";
                return;
            }

            _destSave.Slots[emptySlot] = slot;
            _destSave.Menu.OccupiedSlots[emptySlot] = true;
            _destSave.Menu.SlotData[emptySlot] = slot.MenuData;
            RefreshSlots(DestSlotsPanel, _destSave, false);
            DestSlotCountText.Text = $"({GetOccupiedSlotCount(_destSave)}/10)";
            DestPathText.Text = $"Dropped '{slot.CharName}' to slot {emptySlot}";

            if (from == "bank")
            {
                var entry = _bank.FirstOrDefault(en => en.Slot == slot);
                if (entry != null)
                {
                    BankPanel.Children.Remove(entry.Card);
                    _bank.Remove(entry);
                }
            }
            else if (_sourceSave != null)
            {
                RefreshSlots(SourceSlotsPanel, _sourceSave, true);
            }
        }

        private void SourceSteamIdEditButton_OnClick(object? sender, RoutedEventArgs e)
        {
            if (_sourceSave == null) return;
            SourceSteamIdText.IsVisible = false;
            SourceSteamIdBox.IsVisible = true;
            SourceSteamIdBox.Text = _sourceSave.SteamID.ToString();
            SourceSteamIdEditButton.IsVisible = false;
            SourceSteamIdSaveButton.IsVisible = true;
            SourceSteamIdCancelButton.IsVisible = true;
            SourceSteamIdBox.Focus();
        }

        private void SourceSteamIdSaveButton_OnClick(object? sender, RoutedEventArgs e)
        {
            if (_sourceSave == null) return;
            if (long.TryParse(SourceSteamIdBox.Text, out long steamId))
            {
                try
                {
                    _sourceSave.SteamID = (int)steamId;
                    SourceSteamIdText.Text = steamId.ToString();
                    SourcePathText.Text = $"Steam ID set to {steamId}";
                }
                catch (System.Exception ex)
                {
                    SourceErrorBanner.IsVisible = true;
                    SourceErrorText.Text = $"Error setting Steam ID: {ex.Message}";
                }
            }
            ExitSteamIdEditSource();
        }

        private void SourceSteamIdCancelButton_OnClick(object? sender, RoutedEventArgs e)
        {
            ExitSteamIdEditSource();
        }

        private void ExitSteamIdEditSource()
        {
            SourceSteamIdText.IsVisible = true;
            SourceSteamIdBox.IsVisible = false;
            SourceSteamIdEditButton.IsVisible = true;
            SourceSteamIdSaveButton.IsVisible = false;
            SourceSteamIdCancelButton.IsVisible = false;
        }

        private void DestSteamIdEditButton_OnClick(object? sender, RoutedEventArgs e)
        {
            if (_destSave == null) return;
            DestSteamIdText.IsVisible = false;
            DestSteamIdBox.IsVisible = true;
            DestSteamIdBox.Text = _destSave.SteamID.ToString();
            DestSteamIdEditButton.IsVisible = false;
            DestSteamIdSaveButton.IsVisible = true;
            DestSteamIdCancelButton.IsVisible = true;
            DestSteamIdBox.Focus();
        }

        private void DestSteamIdSaveButton_OnClick(object? sender, RoutedEventArgs e)
        {
            if (_destSave == null) return;
            if (long.TryParse(DestSteamIdBox.Text, out long steamId))
            {
                try
                {
                    _destSave.SteamID = (int)steamId;
                    DestSteamIdText.Text = steamId.ToString();
                    DestPathText.Text = $"Steam ID set to {steamId}";
                }
                catch (System.Exception ex)
                {
                    DestErrorBanner.IsVisible = true;
                    DestErrorText.Text = $"Error setting Steam ID: {ex.Message}";
                }
            }
            ExitSteamIdEditDest();
        }

        private void DestSteamIdCancelButton_OnClick(object? sender, RoutedEventArgs e)
        {
            ExitSteamIdEditDest();
        }

        private void ExitSteamIdEditDest()
        {
            DestSteamIdText.IsVisible = true;
            DestSteamIdBox.IsVisible = false;
            DestSteamIdEditButton.IsVisible = true;
            DestSteamIdSaveButton.IsVisible = false;
            DestSteamIdCancelButton.IsVisible = false;
        }

        private void ShowSaveSuccess(TextBlock textBlock)
        {
            textBlock.IsVisible = true;
            Task.Delay(2500).ContinueWith(t =>
            {
                Dispatcher.UIThread.Post(() => textBlock.IsVisible = false);
            });
        }
    }
}