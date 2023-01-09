<?php declare(strict_types = 0);
/*
** Zabbix
** Copyright (C) 2001-2022 Zabbix SIA
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

<script type="text/javascript">
	$(() => {
		$('#filter-usrgrpid').on('change', () => {
			document.forms['main_filter'].submit();
		});
	});

	const view = new class {

		init({csrf_tokens}) {
			this.csrf_tokens = csrf_tokens;

			this._initActionButtons();
		}

		_initActionButtons() {
			document.addEventListener('click', (e) => {
				let prevent_event = false;

				if (e.target.classList.contains('js-massprovision-user')) {
					prevent_event = !this.massProvisionUser(e.target, Object.keys(chkbxRange.getSelectedIds()));
				}
				else if (e.target.classList.contains('js-massunblock-user')) {
					prevent_event = !this.massUnblockUser(e.target, Object.keys(chkbxRange.getSelectedIds()));
				}
				else if (e.target.classList.contains('js-massdelete-user')) {
					this.massDeleteUser(e.target, Object.keys(chkbxRange.getSelectedIds()));
				}

				if (prevent_event) {
					e.preventDefault();
					e.stopPropagation();
					return false;
				}
			});
		}

		massProvisionUser(target, userids) {
			const confirmation = userids.length > 1
				? <?= json_encode(_('Provision selected LDAP users?')) ?>
				: <?= json_encode(_('Provision selected LDAP user?')) ?>;

			if (!window.confirm(confirmation)) {
				return false;
			}

			create_var(target.closest('form'), '<?= CController::CSRF_TOKEN_NAME ?>',
				this.csrf_tokens['user.provision'], false
			);

			return true;
		}

		massUnblockUser(target, userids) {
			const confirmation = userids.length > 1
				? <?= json_encode(_('Unblock selected users?')) ?>
				: <?= json_encode(_('Unblock selected user?')) ?>;

			if (!window.confirm(confirmation)) {
				return false;
			}

			create_var(target.closest('form'), '<?= CController::CSRF_TOKEN_NAME ?>', this.csrf_tokens['user.unblock'],
				false
			);

			return true;
		}

		massDeleteUser(target, userids) {
			const confirmation = userids.length > 1
				? <?= json_encode(_('Delete selected users?')) ?>
				: <?= json_encode(_('Delete selected user?')) ?>;

			if (!window.confirm(confirmation)) {
				return false;
			}

			create_var(target.closest('form'), '<?= CController::CSRF_TOKEN_NAME ?>', this.csrf_tokens['user.delete'],
				false
			);

			return true;
		}
	};
</script>
