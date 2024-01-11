<?php
/*
** Zabbix
** Copyright (C) 2001-2024 Zabbix SIA
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


class ItemValueWidget {

	const OLD_NAME = 'New widget';

	public static function load() {
		// Create host for aggregation data tests.
		CDataHelper::createHosts([
			[
				'host' => 'Simple host with items for item value widget test',
				'interfaces' => [
					[
						'type' => INTERFACE_TYPE_AGENT,
						'main' => INTERFACE_PRIMARY,
						'useip' => INTERFACE_USE_IP,
						'ip' => '127.0.9.7',
						'dns' => '',
						'port' => '10011'
					]
				],
				'groups' => [
					'groupid' => '4'
				],
				'items' => [
					[
						'name' => 'Item with type of information - numeric (float)',
						'key_' => 'numeric_float',
						'type' => ITEM_TYPE_ZABBIX,
						'value_type' => ITEM_VALUE_TYPE_FLOAT,
						'delay' => '30'
					],
					[
						'name' => 'Item with type of information - Character',
						'key_' => 'character',
						'type' => ITEM_TYPE_ZABBIX,
						'value_type' => ITEM_VALUE_TYPE_STR,
						'delay' => '30'
					],
					[
						'name' => 'Item with type of information - Log',
						'key_' => 'log',
						'type' => ITEM_TYPE_ZABBIX,
						'value_type' => ITEM_VALUE_TYPE_LOG,
						'delay' => '30'
					],
					[
						'name' => 'Item with type of information - numeric (unsigned)',
						'key_' => 'numeric_unsigned',
						'type' => ITEM_TYPE_ZABBIX,
						'value_type' => ITEM_VALUE_TYPE_UINT64,
						'delay' => '30'
					],
					[
						'name' => 'Item with type of information - Text',
						'key_' => 'text',
						'type' => ITEM_TYPE_ZABBIX,
						'value_type' => ITEM_VALUE_TYPE_TEXT,
						'delay' => '30'
					]
				]
			]
		]);
		$itemids = CDataHelper::getIds('name');

		$response = CDataHelper::call('dashboard.create', [
			[
				'name' => 'Dashboard for Single Item value Widget test',
				'pages' => [
					[
						'name' => 'Page with widgets',
						'widgets' => [
							[
								'type' => 'item',
								'name' => self::OLD_NAME,
								'x' => 0,
								'y' => 0,
								'width' => 12,
								'height' => 4,
								'fields' => [
									[
										'type' => ZBX_WIDGET_FIELD_TYPE_ITEM,
										'name' => 'itemid.0',
										'value' => 42230 // Linux: CPU user time.
									],
									[
										'type' => ZBX_WIDGET_FIELD_TYPE_STR,
										'name' => 'description',
										'value' => 'Some description here. Описание.'
									],
									[
										'type' => ZBX_WIDGET_FIELD_TYPE_INT32,
										'name' => 'desc_h_pos',
										'value' => 0
									],
									[
										'type' => ZBX_WIDGET_FIELD_TYPE_INT32,
										'name' => 'desc_v_pos',
										'value' => 0
									],
									[
										'type' => ZBX_WIDGET_FIELD_TYPE_INT32,
										'name' => 'time_h_pos',
										'value' => 2
									],
									[
										'type' => ZBX_WIDGET_FIELD_TYPE_INT32,
										'name' => 'time_v_pos',
										'value' => 2
									],
									[
										'type' => ZBX_WIDGET_FIELD_TYPE_INT32,
										'name' => 'desc_size',
										'value' => 17
									],
									[
										'type' => ZBX_WIDGET_FIELD_TYPE_INT32,
										'name' => 'decimal_size',
										'value' => 41
									],
									[
										'type' => ZBX_WIDGET_FIELD_TYPE_INT32,
										'name' => 'value_size',
										'value' => 56
									],
									[
										'type' => ZBX_WIDGET_FIELD_TYPE_INT32,
										'name' => 'time_size',
										'value' => 14
									]
								]
							],
							[
								'type' => 'item',
								'name' => 'Widget with thresholds',
								'x' => 0,
								'y' => 6,
								'width' => 10,
								'height' => 3,
								'fields' => [
									[
										'type' => ZBX_WIDGET_FIELD_TYPE_ITEM,
										'name' => 'itemid.0',
										'value' => 42230 // Linux: CPU user time.
									],
									[
										'type' => ZBX_WIDGET_FIELD_TYPE_STR,
										'name' => 'thresholds.0.color',
										'value' => 'BF00FF'
									],
									[
										'type' => ZBX_WIDGET_FIELD_TYPE_STR,
										'name' => 'thresholds.0.threshold',
										'value' => '0'
									],
									[
										'type' => ZBX_WIDGET_FIELD_TYPE_STR,
										'name' => 'thresholds.1.color',
										'value' => 'FF0080'
									],
									[
										'type' => ZBX_WIDGET_FIELD_TYPE_STR,
										'name' => 'thresholds.1.threshold',
										'value' => '0.01'
									]
								]
							],
							[
								'type' => 'item',
								'name' => 'Widget to delete',
								'x' => 13,
								'y' => 0,
								'width' => 4,
								'height' => 4,
								'fields' => [
									[
										'type' => ZBX_WIDGET_FIELD_TYPE_ITEM,
										'name' => 'itemid.0',
										'value' => 42230 // Linux: CPU user time.
									]
								]
							]
						]
					]
				]
			],
			[
				'name' => 'Item value widget: Dashboard for zoom filter check',
				'pages' => [
					[
						'name' => 'Page with widgets'
					]
				]
			],
			[
				'name' => 'Item value widget: Dashboard for threshold(s) check',
				'pages' => [
					[
						'name' => 'Page with widgets'
					]
				]
			]
		]);
		$dashboardid = $response['dashboardids'][0];
		$dashboard_zoom = $response['dashboardids'][1];
		$dashboard_threshold = $response['dashboardids'][2];

		return [
			'dashboardid' => $dashboardid,
			'dashboard_zoom' => $dashboard_zoom,
			'dashboard_threshold' => $dashboard_threshold,
			'itemids' => $itemids
		];
	}
}
