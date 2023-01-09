<?php
/*
** Zabbix
** Copyright (C) 2001-2021 Zabbix SIA
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
**/


/**
 * @var CView $this
 */
?>

<script>
	const view = {
		csrf_tokens: null,

		init({csrf_tokens}) {
			this.csrf_tokens = csrf_tokens;

			this._initActionButtons();
		},

		_initActionButtons() {
			document.addEventListener('click', (e) => {
				let prevent_event = false;

				if (e.target.classList.contains('js-massenable-hostprototype')) {
					prevent_event = !this.massEnableHostPrototype(e.target, Object.keys(chkbxRange.getSelectedIds()));
				}
				else if (e.target.classList.contains('js-massdisable-hostprototype')) {
					prevent_event = !this.massDisableHostPrototype(e.target, Object.keys(chkbxRange.getSelectedIds()));
				}
				else if (e.target.classList.contains('js-massdelete-hostprototype')) {
					prevent_event = !this.massDeleteHostPrototype(e.target, Object.keys(chkbxRange.getSelectedIds()));
				}

				if (prevent_event) {
					e.preventDefault();
					e.stopPropagation();
					return false;
				}
			});
		},

		massEnableHostPrototype(target, group_hostids) {
			const confirmation = group_hostids.length > 1
				? <?= json_encode(_('Create hosts from selected prototypes as enabled?')) ?>
				: <?= json_encode(_('Create hosts from selected prototype as enabled?')) ?>;

			if (!window.confirm(confirmation)) {
				return false;
			}

			create_var(target.closest('form'), '<?= CController::CSRF_TOKEN_NAME ?>',
				this.csrf_tokens['hostprototype.massenable'], false
			);

			return true;
		},

		massDisableHostPrototype(target, group_hostids) {
			const confirmation = group_hostids.length > 1
				? <?= json_encode(_('Create hosts from selected prototypes as disabled?')) ?>
				: <?= json_encode(_('Create hosts from selected prototype as disabled?')) ?>;

			if (!window.confirm(confirmation)) {
				return false;
			}

			create_var(target.closest('form'), '<?= CController::CSRF_TOKEN_NAME ?>',
				this.csrf_tokens['hostprototype.massdisable'], false
			);

			return true;
		},

		massDeleteHostPrototype(target, group_hostids) {
			const confirmation = group_hostids.length > 1
				? <?= json_encode(_('Delete selected host prototypes?')) ?>
				: <?= json_encode(_('Delete selected host prototype?')) ?>;

			if (!window.confirm(confirmation)) {
				return false;
			}

			create_var(target.closest('form'), '<?= CController::CSRF_TOKEN_NAME ?>',
				this.csrf_tokens['hostprototype.massdelete'], false
			);

			return true;
		},

		editHost(e, hostid) {
			e.preventDefault();
			const host_data = {hostid};

			this.openHostPopup(host_data);
		},

		openHostPopup(host_data) {
			const original_url = location.href;
			const overlay = PopUp('popup.host.edit', host_data, {
				dialogueid: 'host_edit',
				dialogue_class: 'modal-popup-large',
				prevent_navigation: true
			});

			overlay.$dialogue[0].addEventListener('dialogue.create', this.events.hostSuccess, {once: true});
			overlay.$dialogue[0].addEventListener('dialogue.update', this.events.hostSuccess, {once: true});
			overlay.$dialogue[0].addEventListener('dialogue.delete', this.events.hostDelete, {once: true});
			overlay.$dialogue[0].addEventListener('overlay.close', () => {
				history.replaceState({}, '', original_url);
			}, {once: true});
		},

		events: {
			hostSuccess(e) {
				const data = e.detail;

				if ('success' in data) {
					postMessageOk(data.success.title);

					if ('messages' in data.success) {
						postMessageDetails('success', data.success.messages);
					}
				}

				location.href = location.href;
			},

			hostDelete(e) {
				const data = e.detail;

				if ('success' in data) {
					postMessageOk(data.success.title);

					if ('messages' in data.success) {
						postMessageDetails('success', data.success.messages);
					}
				}

				const curl = new Curl('zabbix.php');
				curl.setArgument('action', 'host.list');

				location.href = curl.getUrl();
			}
		}
	};
</script>
